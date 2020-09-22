/*
 * stun.h
 *
 * STUN protocol (binding request/response)
 *
 * This is not used currently in the NAT traversal process (it was a blind development path)
 * but the utility function for conversion to big endian might come handy later
 *
 *  Created on: Sep 18, 2020
 *      Author: fanick
 */

#ifndef INCLUDE_STUN_H_
#define INCLUDE_STUN_H_


#include <sstream>
#include <vector>
#include <enet/enet.h>

namespace stun {

    template <typename T>
    T toBigEndian(T u){
        union
        {
            T u;
            unsigned char u8[sizeof(T)];
        } source;
        source.u = u;

        T result = 0;

        for (size_t k = 0; k < sizeof(T); k++)
            result |= (source.u8[sizeof(T) - k - 1] << (k * 8));

        return result;
    }
    template <typename T>
    T fromBigEndian(T u){
        union
        {
            T u;
            unsigned char u8[sizeof(T)];
        } source;
        source.u = u;

        T result = 0;

        for (size_t k = 0; k < sizeof(T); k++)
            result |= (source.u8[k] << (k * 8));

        return result;
    }


    #pragma pack(push, 1)
    enum type_t: enet_uint16 {
        BINDING_REQUEST = 0x0001,
        BINDING_RESPONSE = 0x0101
    };

    enum attribute_t: enet_uint16{
        MAPPED_ADDRESS = 0x0001,
        CHANGE_REQUEST = 0x0003,
        SOURCE_ADDRESS = 0x0004
    };

    enum flags_t: enet_uint32 {
        CHANGE_PORT = 0x02,
        CHANGE_ADDRESS = 0x04
    };

    struct changeAttribute {
        attribute_t name = CHANGE_REQUEST;
        enet_uint16 length = 0x04;
        enet_uint32 flags = 0;
    };
    struct addressAttribute {
        attribute_t name;
        enet_uint16 length = 0x08;
        enet_uint16 protocolFamily = 0x01; // IPv4
        enet_uint16 port;
        enet_uint32 address;
    };

    struct message {
        type_t type;
        enet_uint16 length;
        enet_uint8 tranId[16] = {1,2,3,4,5,6,7,8, 9,10,11,12,13,14,15,16};
        std::vector<addressAttribute> attributes;
        std::vector<changeAttribute> changeAttributes;
        addressAttribute mappedAddress;
        addressAttribute sourceAdrress;

        bool read(char * src, size_t len);
        size_t send(char ** dst);

        static constexpr size_t headersSize = sizeof(type) + sizeof(length) + sizeof(tranId);
    };
    #pragma pack(pop)

    bool message::read(char * src, size_t len) {
        if(len < headersSize || len > headersSize + 2 * sizeof (addressAttribute)){
            return false;
        }

        enet_uint16 typeN;
        enet_uint16 lengthN;
        enet_uint8 tranId[16];
        attributes.clear();
        attributes.reserve(4);

        size_t read = 0;
        memcpy(&typeN, src, 2);
        read += 2;
        memcpy(&lengthN, src + read, 2);
        read += 2;
        memcpy(&tranId, src + read, 16);
        read += 16;

        type = (type_t)fromBigEndian(typeN);
        length = fromBigEndian(lengthN);

        size_t remaining = len - read;
        size_t attr = read;
        while (remaining >= sizeof(addressAttribute)){
            read = 0;
            enet_uint16 nameN;
            enet_uint16 lengthN;
            enet_uint8 protocolFamilyN;
            enet_uint16 portN;
            enet_uint32 addressN;

            attribute_t name = (attribute_t)fromBigEndian(nameN);
            addressAttribute & tgt = (name == MAPPED_ADDRESS) ? mappedAddress : sourceAdrress;

            memcpy(&nameN, src + attr + read, 2);
            read += 2;
            memcpy(&lengthN, src + attr + read, 2);
            read += 2;
            memcpy(&protocolFamilyN, src + attr + read, 1);
            read += 1;
            memcpy(&portN, src + attr + read, 2);
            read += 2;
            memcpy(&addressN, src + attr + read, 4);
            read += 4;

            attr += read;
            remaining = remaining - read;

            tgt.name = name;
            tgt.length = fromBigEndian(lengthN);
            tgt.protocolFamily = fromBigEndian(protocolFamilyN);
            tgt.port = fromBigEndian(portN);
            tgt.address = fromBigEndian(addressN);

            attributes.push_back(tgt);
        }

        return true;
    }

    size_t message::send(char **memblockPtr) {
        size_t attributesSize = sizeof(addressAttribute) * attributes.size();
        size_t result = headersSize + attributesSize;
        char *dst = (char*) malloc(result);
        (*memblockPtr) = dst;
        memset(dst, 0, result);
        length = attributesSize;
        enet_uint16 typeN = toBigEndian((enet_uint16)type);
        enet_uint16 lengthN = toBigEndian(length);
        memcpy(dst, &typeN, 2);
        memcpy(dst + 2, &lengthN, 2);
        memcpy(dst + 4, &tranId, 16);

        addressAttribute *attributesPtr = (addressAttribute *) (dst + ((char*) &attributes - (char*) this));
        for (size_t i = 0; i < attributes.size(); i++) {
            addressAttribute & a = attributes[i];
            addressAttribute * adst = &(attributesPtr[i]);
            enet_uint16 nameN = toBigEndian((enet_uint16)a.name);
            enet_uint16 lengthN = toBigEndian(a.length);
            enet_uint16 protocolFamilyN = toBigEndian(a.protocolFamily);
            enet_uint16 portN = toBigEndian(a.port);
            enet_uint32 addressN = a.address;

            memcpy((char*)adst + 0, &nameN, 2);
            memcpy((char*)adst + 2, &lengthN, 2);
            memcpy((char*)adst + 4, &protocolFamilyN, 2);
            memcpy((char*)adst + 6, &portN, 2);
            memcpy((char*)adst + 8, &addressN, 4);
        }
        return result;
    }

}

#endif /* INCLUDE_STUN_H_ */

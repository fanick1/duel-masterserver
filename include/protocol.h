/*
 * protocol.h
 *
 *  Created on: Aug 29, 2020
 *      Author: fanick
 */

#ifndef INCLUDE_PROTOCOL_H_
#define INCLUDE_PROTOCOL_H_


#include <vector>
#include <sstream>
#include <enet/enet.h>

typedef enet_uint32 address_t;
typedef enet_uint16 port_t;

//passed as event.data at conneciton time
// to distinguish requests

enum class REQUEST_TYPE {
    NONE,
    SERVER_REGISTER, // when server connects, it is automatically added to the servers list and then disconnected. server is kept for 30 seconds.
    SERVER_UPDATE,// when server connects, it is automatically updated in the servers list and server update packet is expected
    CLIENT_REQUEST_SERVERLIST, //peer obtaining list of registered servers

    SERVER_NAT_GET_PEERS, //server scrubbing peers wanting to be NAT punched
    CLIENT_NAT_CONNECT_TO_SERVER, // peer requests NAT punch through to given server, this peer is registered as waiting to be scrapped by the server, CLIENT_NAT_PUNCH packet is expected
    COUNT
};

enum PACKET_TYPE {
    SERVER_UPDATE,
    SERVER_LIST,

    SERVER_NAT_PEERS,
    CLIENT_NAT_PUNCH,

    PACKETS_COUNT
};

struct packetHeader {
    enet_uint8 type;
    template<typename Stream>
    bool serialize(Stream &s) {
        return s & type;
    }
};

struct packet_update {
    std::string descr;
    template<typename Stream>
    bool serialize(Stream &s) {
        return s & descr;
    }
};

struct packet_serverlist {
    struct _serverlist_server {
        address_t address = 0;
        port_t port = 0;
        std::string descr;

        template<typename Stream>
        bool serialize(Stream &s) {
            return s & address // @suppress("Suggested parenthesis around expression")
                && s & port
                && s & descr;
        }
    };

    size_t serverCount;
    std::vector<packet_serverlist::_serverlist_server> servers;

    template<typename Stream>
    bool serialize(Stream &s) {
        return s & serverCount
            && s & servers;
    }
};

struct packet_nat_punch {
    // address of the server - must match some server stored in the list, otherwise this packet will not have any effect.
    address_t address = 0;
    port_t port = 0;
    template<typename Stream>
    bool serialize(Stream &s) {
        return s & address
            && s & port;
    }
};

struct packet_nat_peers {
    struct _peer {
        address_t address = 0;
        port_t port = 0;
        template<typename Stream>
        bool serialize(Stream &s) {
            return s & address
                && s & port;
        }
    };

    uint16_t peerCount = 0;
    std::vector<_peer> peers;
    template<typename Stream>
    bool serialize(Stream &s) {
        return s & peerCount
            && s & peers;
    }
};

union hostAddress {
    address_t address;
    uint8_t a[4];
};

std::string hostToIPaddress(address_t a, port_t p){
    std::ostringstream is;
    hostAddress hostAddress;
    hostAddress.address = a;

    is << (uint16_t) hostAddress.a[0];
    is << ".";
    is << (uint16_t) hostAddress.a[1];
    is << ".";
    is << (uint16_t) hostAddress.a[2];
    is << ".";
    is << (uint16_t) hostAddress.a[3];
    is << ":";
    is << p;
    return is.str();
}

#endif /* INCLUDE_PROTOCOL_H_ */

/*
 * master.h
 *
 *  Created on: Aug 28, 2020
 *      Author: fanick
 */

#ifndef INCLUDE_SERIALIZE_H_
#define INCLUDE_SERIALIZE_H_

#include <vector>
#include <sstream>
#include <type_traits>

namespace masterserver {

#define SIZEBYTES_LIMIT 4
#define UINT24_MAX ((1 << 24) - 1)

    template<typename Stream, typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    bool operator <<(Stream &s, T t) {
        return s.write((unsigned char*) &t, sizeof(T));
    }

    template<typename Stream, typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    bool operator >>(Stream &s, T &t) {
        return s.read((unsigned char*) &t, sizeof(T));
    }
    template<typename Stream, typename T, typename std::enable_if<std::is_class<T>::value, int>::type = 0>
    bool operator <<(Stream &stream, T &t) {
        return t.serialize(stream);
    }
    template<typename Stream, typename T, typename std::enable_if<std::is_class<T>::value, int>::type = 0>
    bool operator >>(Stream &stream, T &t) {
        return t.serialize(stream);
    }

    template<typename Stream>
    bool operator <<(Stream &s, std::string &t) {
        uint32_t size = t.length();
        writeSize(s, size);
        s.write((unsigned char*) t.c_str(), size);
        return s.good();
    }

    template<typename Stream>
    bool operator >>(Stream &s, std::string &t) {
        uint32_t size = readSize(s);
        if(size > 255){
            return false;
        }
        t.reserve(size);
        unsigned char data[size + 1] = { 0 };
        s.read(data, size);
        t.assign((char*) data, size);
        return s.good();
    }

    template<typename Stream, typename ... Ts>
    bool operator <<(Stream &s, std::vector<Ts...> &t) {
        uint32_t size = t.size();
        writeSize(s, size);
        for (auto &e : t) {
            s << e;
        }
        return s.good();
    }

    template<typename Stream, typename ... Ts>
    bool operator <<(Stream &s, const std::vector<Ts...> &t) {
        uint32_t size = t.size();
        writeSize(s, size);
        for (auto &e : t) {
            s << e;
        }
        return s.good();
    }

    template<typename Stream, typename T>
    bool operator >>(Stream &s, std::vector<T> &t) {
        uint32_t size = readSize(s);
        if(size > 1000){
            return false;
        }
        t.clear();
        t.reserve(size);
        for (size_t i = 0; i < size; i++) {
            T item;
            s >> item;
            if (s.good()) {
                t.push_back(item);
            } else {
                return false;
            }
        }
        return s.good();
    }

    template<typename Stream, typename T>
    bool read(Stream &stream, T &t) {
        return stream >> t;
    }

    template<typename Stream, typename T>
    bool write(Stream &stream, T &t) {
        return stream << t;
    }

    template<typename Stream, typename T>
    bool serialize(Stream &stream, T &t) {
        if constexpr (Stream::isDeserializer()) {
            return read(stream, t);
        } else {
            return write(stream, t);
        }
    }

    template<typename Stream>
//writes header for string, lists etc...
    void writeSize(Stream &s, uint32_t size) {
        uint8_t sizeBytes = 0;
        if (size > UINT8_MAX - 4) {
            sizeBytes = 1;
        }
        if (size > UINT8_MAX) {
            sizeBytes = 2;
        }
        if (size > UINT16_MAX) {
            sizeBytes = 3;
        }
        if (size > UINT24_MAX) {
            sizeBytes = 4;
        }
        if (sizeBytes > 0) {
            uint8_t sizeByte = sizeBytes + UINT8_MAX - 4;
            s.write((unsigned char*) &sizeByte, 1);
            s.write((unsigned char*) &size, sizeBytes);
        } else {
            s.write((unsigned char*) &size, 1);
        }
    }
    template<typename Stream>
    //TODO return error state
    uint32_t readSize(Stream &s) {
        uint32_t size = 0;
        uint8_t sizeBytes = 0;
        s.read((unsigned char*) &size, 1);    //TODO add good() check
        if (size > UINT8_MAX - 4) {
            sizeBytes = size - (UINT8_MAX - 4);
            s.read((unsigned char*) &size, sizeBytes);    //TODO add good() check
        }
        return size;
    }

    struct serializer {
        std::basic_ostringstream<unsigned char> os;
        typedef std::basic_ostringstream<unsigned char>::__string_type string_type;

        serializer() {
        }

        long getDataLen() {
            return os.tellp();
        }

        string_type getData() {
            return os.str();
        }

        template<typename M>
        bool operator &(const M &m) {
            return serialize(*this, m);
        }
        template<typename M>
        bool operator &(M &m) {
            return serialize(*this, m);
        }

        bool write(unsigned char *src, size_t len) {
            os.write(src, len);
            return os.good();
        }

        bool good() {
            return os.good();
        }
        constexpr static bool isSerializer() {
            return true;
        }
        constexpr static bool isDeserializer() {
            return false;
        }
    };

    struct deserializer {
        std::basic_istringstream<unsigned char> is;
        typedef std::basic_istringstream<unsigned char>::__string_type string_type;

        deserializer(const string_type &s)
            : is(s) {
        }

        deserializer(unsigned char *data, size_t datalen)
            : is(std::basic_string(data, datalen)) {
        }

        template<typename M>
        bool operator &(M &m) {
            return *this >> m;
        }

        bool read(unsigned char *dst, size_t len) {
            is.read(dst, len);
            return is.good();
        }

        bool good() {
            return is.good();
        }
        constexpr static bool isSerializer() {
            return false;
        }
        constexpr static bool isDeserializer() {
            return true;
        }
    };
}
#endif /* INCLUDE_MASTER_H_ */

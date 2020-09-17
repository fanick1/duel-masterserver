#ifndef DUEL_MASTERSERVER_H
#define DUEL_MASTERSERVER_H
#include <tuple>
#include <deque>
#include <enet/enet.h>
#include "protocol.h"
#include "serialize.h"
#include <chrono>

extern std::chrono::steady_clock::time_point now;

typedef std::tuple<address_t, port_t> server_address_t;
typedef std::tuple<address_t, port_t, address_t, port_t> peer_address_t;
typedef std::map<peer_address_t, std::chrono::steady_clock::time_point> peer_nat_map_t;

enum class PEER_MODE {
    NONE,
    SERVER,
    CLIENT
};

struct peer_entry {
    PEER_MODE mode = PEER_MODE::NONE;
    std::chrono::steady_clock::time_point validUntil;

    peer_entry(PEER_MODE mode, std::chrono::steady_clock::time_point validUntil)
        : mode(mode),
          validUntil(validUntil) {
    }
};

struct server_list_entry {
    address_t address = 0;
    port_t port = 0;
    address_t localNetworkAddress = 0;
    port_t localNetworkPort = 0;
    address_t publicIPAddress = 0;
    port_t publicPort = 0;
    bool needsNAT = false;
    std::string descr = "some description";

    peer_nat_map_t natClients;
    std::chrono::steady_clock::time_point validUntil;

    bool deleted = true;

    void registerNatClient(address_t a, port_t p, address_t localAddress, port_t localPort) {
        if(natClients.size() > 10){
            return;
        }
        auto x = now + std::chrono::seconds(50);
        auto k = std::make_tuple(a, p, localAddress, localPort);
        natClients[k] = x;
    }
    std::vector<peer_address_t> scrubNATClients(){
        std::vector<peer_address_t> result;
        result.reserve(natClients.size());
        for(auto & e : natClients){
            result.push_back(e.first);
        }
        natClients.clear();
        return result;
    }
};

struct entryMap {
    std::map<server_address_t, server_list_entry> mapa;

    void update(address_t address, port_t port, const std::string &descr,
                address_t localAddress, port_t localPort,
                address_t publicIPAddress, port_t publicPort,
                bool needsNAT) {
        server_address_t k = std::make_tuple(address, port);
        server_list_entry &e = mapa[k];
        e.descr = descr;
        e.localNetworkAddress = localAddress;
        e.localNetworkPort = localPort;
        e.publicIPAddress = publicIPAddress;
        e.publicPort = publicPort;
        e.needsNAT = needsNAT;
    }

    bool has(address_t address, port_t port) {
        server_address_t k = std::make_tuple(address, port);
        return mapa.count(k);
    }

    server_list_entry& get(address_t address, port_t port) {
        server_address_t k = std::make_tuple(address, port);
        return mapa[k];
    }

    void refresh(address_t address, port_t port) {
        server_list_entry &e = get(address, port);
        e.validUntil = now + std::chrono::seconds(60);
        e.deleted = false;
        e.address = address;
        e.port = port;
    }

    void purgeOld() {
        for (auto it = mapa.begin(); it != mapa.end();){
            auto & natClients = it->second.natClients;
            for(auto natit = natClients.cbegin(); natit != natClients.cend();){
                if (natit->second < now){
                    natit = it->second.natClients.erase(natit);
                } else {
                    natit++;
                }
            }
            if (it->second.validUntil < now) {
                it = mapa.erase(it);
            }
            else {
                ++it;
            }
        }

    }

    void getValidHosts(std::list<server_list_entry*> &result) {
        for (auto &e : mapa) {
            if (!e.second.deleted) {
                if (e.second.validUntil < now) {
                    e.second.deleted = true;
                } else {
                    result.push_back(&e.second);
                }
            }
        }

    }
};
#endif

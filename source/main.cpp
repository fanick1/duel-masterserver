#include <iostream>
#include <string>
#include <cstdint>
#include <chrono>
#include <ctime>
#include <map>
#include <tuple>
#include <list>
#include <cstring>
#include <enet/enet.h>
#include "../include/masterserver.h"

std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

entryMap hostList;

void addNATPeer(ENetPeer *peer, address_t address, port_t port) {
    if (!hostList.has(address, port)) {
        printf("The NAT punch request refers to unknown server %s !\n", hostToIPaddress(address, port).c_str());
        return;
    }
    server_list_entry &e = hostList.get(address, port);
    e.registerNatClient(peer->address.host, peer->address.port);
}

void sendWaitingNATPeersToServer(ENetPeer *server) {
    server_list_entry &e = hostList.get(server->address.host, server->address.port);
    masterserver::serializer s;
    packetHeader header;
    header.type = PACKET_TYPE::SERVER_NAT_PEERS;
    s << header;

    packet_nat_peers p;
    std::vector<peer_address_t> addresses = e.scrubNATClients();
    for (auto &c : addresses) {
        packet_nat_peers::_peer peer;
        peer.address = std::get<0>(c);
        peer.port = std::get<1>(c);
        p.peers.push_back(peer);
    }
    p.peerCount = p.peers.size();

    s << p;
    ENetPacket *enetPacket = enet_packet_create(s.getData().c_str(), s.getDataLen(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(server, 0, enetPacket);
}

void sendHostsToPeer(ENetPeer *peer) {
    static std::list<server_list_entry*> validHosts;
    validHosts.clear();
    hostList.getValidHosts(validHosts);
    auto serverCount = validHosts.size();

    masterserver::serializer s;
    packetHeader header;
    header.type = PACKET_TYPE::SERVER_LIST;
    s << header;
    packet_serverlist p;

    p.serverCount = serverCount;
    p.servers.reserve(serverCount);

    for (server_list_entry *e : validHosts) {
        packet_serverlist::_serverlist_server server;
        server.address = e->address;
        server.port = e->port;
        server.descr = e->descr;
        p.servers.push_back(server);
    }
    s << p;
    long dataLen = s.getDataLen();
    ENetPacket *enetPacket = enet_packet_create(s.getData().c_str(), dataLen, ENET_PACKET_FLAG_RELIABLE);

    enet_peer_send(peer, 0, enetPacket);
}

void onPacketReceived(ENetPeer *peer, ENetPacket *p) {
    masterserver::deserializer d(p->data, p->dataLength);
    packetHeader header;
    d >> header;

    switch (header.type) {
        case PACKET_TYPE::SERVER_UPDATE: {
            printf("It's a server update!\n");
            packet_update s;
            d >> s;
            hostList.updateDescr(peer->address.host, peer->address.port, s.descr);
            break;
        }
        default:
            break;
    }
}
void onPeerPacketReceived(ENetPeer *peer, ENetPacket *p) {
    masterserver::deserializer d(p->data, p->dataLength);
    packetHeader header;
    d >> header;

    switch (header.type) {
        case PACKET_TYPE::CLIENT_NAT_PUNCH: {
            printf("It's a NAT punch request!\n");
            packet_nat_punch s;
            d >> s;
            addNATPeer(peer, s.address, s.port);
            enet_peer_disconnect_later(peer, 0);
            break;
        }

        default:
            break;
    }
}

int main() {
    ENetAddress address;
    ENetHost *server;
    /* Bind the server to the default localhost.     */
    /* A specific host address can be specified by   */
    /* enet_address_set_host (& address, "x.x.x.x"); */
    address.host = ENET_HOST_ANY;
    /* Bind the server to port 1234. */
    address.port = 5902;
    server = enet_host_create(&address /* the address to bind the server host to */,
        32 /* allow up to 32 clients and/or outgoing connections */,
        1 /* allow up to 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);
    if (server == NULL)
    {
        std::cerr << "An error occurred while trying to create an ENet server host.\n";
        exit(EXIT_FAILURE);
    }

    ENetEvent event;

    for (;;) {
        now = std::chrono::steady_clock::now();
        hostList.purgeOld();
        for (size_t i = 0; i < server->peerCount; i++) {
            ENetPeer *p = &server->peers[i];
            peer_entry *pe = (peer_entry*) p->data;
            if (p->state == ENetPeerState::ENET_PEER_STATE_CONNECTED && pe->validUntil < now) {
                enet_peer_disconnect_later(p, 0);
            }
        }

        while (enet_host_service(server, &event, 100) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_NONE:
                    break;
                case ENET_EVENT_TYPE_CONNECT: {
                    REQUEST_TYPE rt = REQUEST_TYPE::NONE;
                    if (event.data < static_cast<int>(REQUEST_TYPE::COUNT)) {
                        rt = static_cast<REQUEST_TYPE>(event.data);
                    } else {
                        enet_peer_disconnect(event.peer, 255);
                        continue;
                    }
                    switch (rt) {
                        case REQUEST_TYPE::SERVER_REGISTER: {
                            printf("server %s connected \n", hostToIPaddress(event.peer->address.host, event.peer->address.port).c_str());
                            event.peer->data = (void*) new peer_entry(PEER_MODE::SERVER, now + std::chrono::seconds(5));
                            hostList.refresh(event.peer->address.host, event.peer->address.port);
                            enet_peer_disconnect(event.peer, 0);
                            break;
                        }
                        case REQUEST_TYPE::SERVER_UPDATE: {
                            printf("server %s connected \n", hostToIPaddress(event.peer->address.host, event.peer->address.port).c_str());
                            event.peer->data = (void*) new peer_entry(PEER_MODE::SERVER, now + std::chrono::seconds(5));
                            hostList.refresh(event.peer->address.host, event.peer->address.port);
                            break;
                        }
                        case REQUEST_TYPE::CLIENT_REQUEST_SERVERLIST: {
                            printf("peer %s requesting server list \n", hostToIPaddress(event.peer->address.host, event.peer->address.port).c_str());
                            event.peer->data = (void*) new peer_entry(PEER_MODE::CLIENT, now + std::chrono::seconds(1));
                            sendHostsToPeer(event.peer);
                            enet_peer_disconnect_later(event.peer, 0);
                            break;
                        }
                        case REQUEST_TYPE::SERVER_NAT_GET_PEERS: {
                            printf("server %s requesting peers for NAT punch through \n", hostToIPaddress(event.peer->address.host, event.peer->address.port).c_str());
                            event.peer->data = (void*) new peer_entry(PEER_MODE::SERVER, now + std::chrono::seconds(5));
                            hostList.refresh(event.peer->address.host, event.peer->address.port);
                            sendWaitingNATPeersToServer(event.peer);
                            enet_peer_disconnect_later(event.peer, 0);
                            break;
                        }
                        case REQUEST_TYPE::CLIENT_NAT_CONNECT_TO_SERVER: {
                            printf("peer %s requesting NAT punch\n", hostToIPaddress(event.peer->address.host, event.peer->address.port).c_str());
                            event.peer->data = (void*) new peer_entry(PEER_MODE::CLIENT, now + std::chrono::seconds(5));

                            break;
                        }

                        case REQUEST_TYPE::NONE:
                            case REQUEST_TYPE::COUNT:
                            default:
                            enet_peer_disconnect_later(event.peer, 0);
                            break;
                    }

                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                    delete ((peer_entry*) event.peer->data);
                    event.peer->data = NULL;
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    peer_entry *pe = (peer_entry*) event.peer->data;
                    switch (pe->mode) {
                        case PEER_MODE::SERVER: {
                            onPacketReceived(event.peer, event.packet);
                            break;
                        }
                        case PEER_MODE::CLIENT: {
                            onPeerPacketReceived(event.peer, event.packet);
                            break;
                        }
                        case PEER_MODE::NONE: {
                            break;
                        }
                    }
                    /* Clean up the packet now that we're done using it. */
                    enet_packet_destroy(event.packet);
                    break;
            }
        }

    }

    return 0;
}


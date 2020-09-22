/**
 * testing tool for the masterserver
 */

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
#include "../include/serialize.h"
#include "../include/protocol.h"

void onPacketReceived(ENetPacket *p) {
    masterserver::deserializer d(p->data, p->dataLength);
    packetHeader header;
    d >> header;

    switch (header.type) {
        case PACKET_TYPE::SERVER_UPDATE: {
            //nope
            break;
        }
        case PACKET_TYPE::SERVER_LIST: {
            printf("It's a server list!\n");
            packet_serverlist s;
            d >> s;
            for (auto &server : s.servers) {
                printf(" descr: %s , addr: %s\n", server.descr.c_str(), hostToIPaddress(server.address, server.port).c_str());
            }
            break;
        }

        case PACKET_TYPE::SERVER_NAT_PEERS: {
            printf("NAT peers received!\n");
            packet_nat_peers p;
            d >> p;
            for (auto &peer : p.peers) {
                printf(" addr: %s\n", hostToIPaddress(peer.address, peer.port).c_str());
            }
            break;
        }
        default:
            break;
    }

}
void sendUpdate(ENetPeer *peer) {
    packet_update p;
    masterserver::serializer s;
    packetHeader header;
    header.type = PACKET_TYPE::SERVER_UPDATE;
    p.descr = "FRANTAA";
    s << header;
    s << p;
    ENetPacket *enetPacket = enet_packet_create(s.getData().c_str(), s.getDataLen(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, enetPacket);
}
void sendTestPacket(ENetPeer *peer, bool testServer, bool testNat) {
    bool send = false;
    packetHeader header;
    masterserver::serializer s;

    if (testServer) {
        if (testNat) {

            send = false;
        } else {
            send = false;
        }
    } else {
        if (testNat) {
            packet_nat_punch p;
            p.address = peer->address.host; // just for testing to get the 127.0.0.1 right
            p.port = 5910;
            header.type = CLIENT_NAT_PUNCH;
            s << header;
            s << p;
            send = true;
        } else {
            send = false;
        }
    }

    if (!send) {
        return;
    }

    ENetPacket *enetPacket = enet_packet_create(s.getData().c_str(), s.getDataLen(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, enetPacket);
}
int main(int argc, char *argv[]) {
    std::string arg1;
    std::string arg2;
    bool testServer = false;
    bool anyport = false;
    bool testNat = false;

    if (argc > 1) {
        arg1 = std::string(argv[1]);
    }
    if (argc > 2) {
        arg2 = std::string(argv[2]);
    }
    if (arg1 == "server") {
        testServer = true;
    }
    if (arg1 == "nat" || arg2 == "nat") {
        testNat = true;
    }

    if (arg2 == "any"){
        anyport = true;
    }
    ENetAddress address;

    /* Bind the server to the default localhost.     */
    /* A specific host address can be specified by   */
    /* enet_address_set_host (& address, "x.x.x.x"); */
    address.host = ENET_HOST_ANY;
    /* Bind the server to port 1234. */
    address.port = testServer ? anyport ? ENET_PORT_ANY : 5910 : ENET_PORT_ANY;

    ENetHost *client;
    client = enet_host_create(&address /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        1 /* allow up 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);
    if (client == NULL)
    {
        fprintf(stderr,
            "An error occurred while trying to create an ENet client host.\n");
        exit(EXIT_FAILURE);
    }
    ENetEvent event;
    ENetPeer *peer;

    enet_address_set_host(&address, "127.0.0.1");  // <----   targetHost with the master server here
    address.port = 25900;

    /** Testing code ahead - retry connection where more packets are actually sent for NAT punch */
    bool connected = false;

    for (int retry = 0; retry < 5 && !connected; retry++) {
        printf("Retry:%i \n", retry);
        REQUEST_TYPE requestType = REQUEST_TYPE::NONE;

        if (testServer) {
            requestType = REQUEST_TYPE::SERVER_REGISTER;
            if (testNat) {
                requestType = REQUEST_TYPE::SERVER_NAT_GET_PEERS;
            }
        } else {
            requestType = REQUEST_TYPE::CLIENT_REQUEST_SERVERLIST;
            if (testNat) {
                requestType = REQUEST_TYPE::CLIENT_NAT_CONNECT_TO_SERVER;
            }
        }
        peer = enet_host_connect(client, &address, 1, static_cast<enet_uint32>(requestType));
        if (peer == NULL) {
            fprintf(stderr,
                "No available peers for initiating an ENet connection.\n");
            exit(EXIT_FAILURE);
        }
        enet_peer_timeout(peer, 500, 200, 1000);

        while (enet_host_service(client, &event, 1000) > 0 && !connected) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                printf("Connected now. \n");
                connected = true;
                break;
            }
        }

        if (!connected) {
            enet_peer_reset(peer);
        }
    }
    if (!connected) {
        enet_peer_reset(peer);
        puts("Connection to 127.0.0.1:5902 failed."); //TODO reflect address and port
        return 1;

    }
    puts("Connection to 127.0.0.1:5902 succeeded."); //TODO reflect address and port
    bool updateSent = false;
    bool testPacketSent = false;
    for (;;) {

        bool serviced = false;
        if (testServer && !updateSent) {
            sendUpdate(peer);
            updateSent = true;
        }
        if (!testPacketSent) {
            sendTestPacket(peer, testServer, testNat);
            testPacketSent = true;
        }

        while (enet_host_service(client, &event, 10) > 0) {

            switch (event.type)
            {
                case ENET_EVENT_TYPE_NONE:
                    break;
                case ENET_EVENT_TYPE_CONNECT:
                    printf("Connect again... should not happen.\n");
                    connected = true;
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    printf("Disconnected... .\n");
                    connected = false;
                    return 0;
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    printf("A packet of length %lu was received on channel %u.\n",
                        event.packet->dataLength,
                        event.channelID);

                    onPacketReceived(event.packet);
                    /* Clean up the packet now that we're done using it. */
                    enet_packet_destroy(event.packet);

                    break;
            }
        }
    }

    return 0;
}


/**
 * dev tool (client side) for figuring out the NAT intricacies
 */

#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>
#include <chrono>
#include <ctime>
#include <map>
#include <tuple>
#include <list>
#include <cstring>
#include <enet/enet.h>

std::string addressToStr(enet_uint32 a) {
    union hostAddress {
        enet_uint32 address;
        uint8_t a[4];
    };
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
    return is.str();
}

int main(int argc, char *argv[]) {
    std::string flareHost = "example.com";

    if (argc > 1) {
        flareHost = std::string(argv[1]);
    }

    ENetAddress address;

    /* Bind the server to the default localhost.     */
    address.host = ENET_HOST_ANY;

    for (size_t currentPort = 5902; currentPort < 6000; currentPort++) {

        address.port = currentPort;

        ENetHost *server;

        server = enet_host_create(&address /* create a server host */,
            10,
            1 /* allow up 2 channels to be used, 0 and 1 */,
            0 /* assume any amount of incoming bandwidth */,
            0 /* assume any amount of outgoing bandwidth */);
        if (server == NULL)
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet client host.\n");
            exit(EXIT_FAILURE);
        }
        ENetEvent event;

        ENetAddress flareAddress;
        flareAddress.port = 5902;
        enet_address_set_host(&flareAddress, flareHost.c_str());
        ENetAddress flareAddress2;
        flareAddress2.port = 15903;
        enet_address_set_host(&flareAddress2, flareHost.c_str());
        ENetAddress flareAddress3;
        flareAddress3.port = 25904;
        enet_address_set_host(&flareAddress3, flareHost.c_str());
        ENetPeer *peer1 = enet_host_connect(server, &flareAddress, 1, currentPort);
        ENetPeer *peer2 = enet_host_connect(server, &flareAddress2, 1, currentPort);
        ENetPeer *peer3 = enet_host_connect(server, &flareAddress3, 1, currentPort);
        bool dis1 = true, dis2 = true, dis3 = true;
        bool con1 = false, con2 = false, con3 = false;
        while (dis1 || dis2 || dis3) {
            if (con1 && con2 && con3) {
                enet_peer_disconnect(peer1, 0);
                enet_peer_disconnect(peer2, 0);
                enet_peer_disconnect(peer3, 0);
            }
            while (enet_host_service(server, &event, 0) > 0) {
                switch (event.type)
                {
                case ENET_EVENT_TYPE_NONE:
                    break;
                case ENET_EVENT_TYPE_CONNECT: {
                    printf("Connected\n");
                    if (event.peer == peer1) con1 = true;
                    if (event.peer == peer2) con2 = true;
                    if (event.peer == peer3) con3 = true;
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT: {
                    printf("Disconnected... .\n");
                    if (event.peer == peer1) dis1 = false;
                    if (event.peer == peer2) dis2 = false;
                    if (event.peer == peer3) dis3 = false;
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE: {
                    enet_packet_destroy(event.packet);

                    break;
                }
                }
            }
        }
        enet_host_destroy(server);
    }

    return 0;
}

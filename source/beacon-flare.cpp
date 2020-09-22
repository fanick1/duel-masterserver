/**
 * dev tool (server part) for figuring out the NAT intricacies
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
#include <vector>

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
ENetHost* create(enet_uint16 port) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;
    ENetHost *server;
    server = enet_host_create(&address /* create a server host */,
        100,
        1,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);
    if (server == NULL)
    {
        fprintf(stderr,
            "An error occurred while trying to create an ENet client host.\n");
        exit(EXIT_FAILURE);
    }
    return server;
}
int main(int argc, char *argv[]) {

    // open 3 sockets on these ports
    std::vector<ENetHost*> servers = {
    create(5902),
    create(15903),
    create(25904)
    };

    ENetEvent event;
    while (true) {
        for (ENetHost *server : servers) {
            bool serviced = false;
            do {
                if (!serviced && enet_host_service(server, &event, 0) <= 0) break;
                serviced = true;
                enet_uint32 sourcePort;
                switch (event.type) {
                case ENET_EVENT_TYPE_NONE:
                    break;
                case ENET_EVENT_TYPE_CONNECT: {
                    sourcePort = event.data;
                    printf("Peer: %u   %s:%u / (src port %u)\n", server->address.port, addressToStr(event.peer->address.host).c_str(),
                        event.peer->address.port, sourcePort);
                    enet_peer_disconnect_later(event.peer, 0);
                    break;
                }

                case ENET_EVENT_TYPE_DISCONNECT:
                    break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    enet_packet_destroy(event.packet);
                    break;
                }
                }
            } while (enet_host_check_events(server, &event) > 0);
        }
    }
    return 0;
}

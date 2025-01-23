#ifndef CLIENT
#define CLIENT

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdint>
#include <fstream>

#include <sys/socket.h>
#include <netinet/in.h>
#include "../include/packet_t.h"

class Client{
private:
    int socket_p;
public:
    Client();
    ~Client();

    void socket_init();
    size_t client_send_and_receive(const int&,
                                const char*,
                                const std::vector<Packet_t::dgram_t>&,
                                std::ofstream&,
                                size_t,
                                const int&);
};
#endif
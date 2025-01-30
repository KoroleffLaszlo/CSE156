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

class Client{
private:
    int socket_p;
public:
    Client();
    ~Client();

    struct dgram_t{
        uint16_t sequence_num;
        std::vector<uint8_t> data_body;
    };
    
    void socket_init();
    size_t client_send_and_receive(const int&,
                                const char*,
                                const std::vector<Client::dgram_t>&,
                                std::ofstream&,
                                size_t,
                                const int&);
    std::vector<uint8_t> encode_dgram(uint16_t);
    void fileRead(const std::string&,
                const std::string&,
                const char*,
                const int&,
                const int&);
};
#endif
#ifndef DATAGRAM
#define DATAGRAM

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>

class Datagram{
private:
    struct dgram_t{
        uint16_t seq_num;
        uint8_t ack;
        std::vector<uint8_t> data_body;
    };

public:
    Client();
    ~Client();
    
    std::vector<uint8_t> encode_dgram(uint16_t);
    uint16_t decode_dgram(std::vector<uint8_t>);
};
#endif
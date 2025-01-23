#ifndef PACKET_T
#define PACKET_T

#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>

class Packet_t{
public:
    struct dgram_t{
        uint16_t sequence_num;
        std::vector<uint8_t> data_body;
    };
    
    std::vector<uint8_t> encode_dgram(uint16_t);
};
#endif
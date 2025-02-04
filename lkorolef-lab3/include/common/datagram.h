#ifndef DATAGRAM
#define DATAGRAM

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <utility>

#include <sys/socket.h>
#include <netinet/in.h>

class Dgram{
private:
    struct dgram_t{ //package datagram
        uint16_t seq_num;
        uint8_t ack;
        std::vector<uint8_t> data_body;
    };

public:
    std::vector<uint8_t> encode_bytes(uint16_t);
    uint16_t decode_bytes(std::vector<uint8_t>);
    std::vector<uint8_t> encode_meta_packet(uint16_t, std::string);
    std::pair<uint16_t, std::string> decode_meta_packet(std::vector<uint8_t>);
    std::vector<uint8_t> encode_ack_packet(uint16_t);
    uint16_t decode_ack_packet(std::vector<uint8_t>);
};
#endif
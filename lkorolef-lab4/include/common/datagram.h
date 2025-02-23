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
public:

    std::vector<uint8_t> encode_bytes(uint32_t);
    uint32_t decode_bytes(std::vector<uint8_t>);
    std::vector<uint8_t> encode_data_packet(uint32_t, std::vector<uint8_t>);
    std::vector<uint8_t> encode_meta_packet(uint32_t, std::string);
    std::pair<uint32_t, std::string> decode_meta_packet(std::vector<uint8_t>);
    std::vector<uint8_t> encode_ack_packet(uint32_t);
    uint32_t decode_ack_packet(std::vector<uint8_t>);
    std::vector<uint8_t> encode_fin_packet(uint32_t);
    uint32_t decode_fin_packet(std::vector<uint8_t>);
};
#endif
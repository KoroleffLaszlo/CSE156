#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <utility>

#define META_FLAG 0
#define ACK_FLAG 2

#include "../../include/common/datagram.h"

std::vector<uint8_t> Dgram::encode_bytes(uint16_t seq_num){
    uint16_t seq_network_order = htons(seq_num);
    return{
        static_cast<uint8_t>(seq_network_order >> 8),
        static_cast<uint8_t>(seq_network_order & 0xFF)
    };
}

uint16_t Dgram::decode_bytes(std::vector<uint8_t> seq_num){
    uint16_t seq_network_order = static_cast<uint16_t>(seq_num[0]) << 8 | static_cast<uint16_t>(seq_num[1]);
    return htons(seq_network_order);
}

// for client
std::vector<uint8_t> Dgram::encode_meta_packet(uint16_t winsz, std::string filename){
    std::vector<uint8_t> meta_packet = {
        static_cast<uint8_t>(META_FLAG),
        static_cast<uint8_t>(winsz >> 8),
        static_cast<uint8_t>(winsz & 0xFF),
    };
    meta_packet.insert(meta_packet.end(), filename.begin(), filename.end());
    return meta_packet;
}

// for server 
// returns winsz, and filepath
std::pair<uint16_t, std::string> Dgram::decode_meta_packet(std::vector<uint8_t> packet){
    // ???TODO:use htons to insure proper byte ordering from client
    std::string filePath(packet.begin() + 2, packet.end());
    return std::make_pair(
        static_cast<uint16_t>(packet[1]) << 8 | static_cast<uint16_t>(packet[2]), //winsize
        filePath);
}

std::vector<uint8_t> Dgram::encode_ack_packet(uint16_t seq_num){
    std::vector<uint8_t>ack_packet = encode_bytes(seq_num);
    ack_packet.insert(ack_packet.begin(), static_cast<uint8_t>(ACK_FLAG));
    return ack_packet;
}

uint16_t Dgram::decode_ack_packet(std::vector<uint8_t>ack_packet){
    uint16_t seq_num = decode_bytes(std::vector<uint8_t>(ack_packet.begin() + 1, ack_packet.end()));
    return seq_num;
}
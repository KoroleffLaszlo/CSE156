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
#include <bitset>

#define META_FLAG 0
#define DATA_FLAG 1
#define ACK_FLAG 2
#define FIN_FLAG 3

#include "../../include/common/datagram.h"

std::vector<uint8_t> Dgram::encode_bytes(uint32_t seq_num){
    return{
        static_cast<uint8_t>(seq_num>> 24),
        static_cast<uint8_t>(seq_num >> 16),
        static_cast<uint8_t>(seq_num >> 8),
        static_cast<uint8_t>(seq_num & 0xFF)
    };
}

uint32_t Dgram::decode_bytes(std::vector<uint8_t> seq_num){
    uint32_t seq_network_order = static_cast<uint32_t>(seq_num[0]) << 24 |
                    static_cast<uint32_t>(seq_num[1]) << 16 |
                    static_cast<uint32_t>(seq_num[2]) << 8 |
                    static_cast<uint32_t>(seq_num[3]);
    return seq_network_order;
}

std::vector<uint8_t> Dgram::encode_data_packet(uint32_t seq_num, std::vector<uint8_t> data){
    std::vector<uint8_t> encode_pack = encode_bytes(seq_num);
    encode_pack.insert(encode_pack.end(), data.begin(), data.end());
    encode_pack.insert(encode_pack.begin(), DATA_FLAG);
    return encode_pack;
}

// for client
std::vector<uint8_t> Dgram::encode_meta_packet(uint32_t winsz, std::string filename){
    std::vector<uint8_t> meta_packet = encode_bytes(winsz);
    meta_packet.insert(meta_packet.begin(), META_FLAG);
    meta_packet.insert(meta_packet.end(), filename.begin(), filename.end());
    meta_packet.push_back('\0');
    return meta_packet;
}

// for server 
// returns winsz, and filepath
std::pair<uint32_t, std::string> Dgram::decode_meta_packet(std::vector<uint8_t> packet){
    std::string filePath(packet.begin() + 5, packet.end());
    uint32_t winsz = decode_bytes(std::vector<uint8_t>(packet.begin() + 1, packet.begin() + 5));
    return std::make_pair(
        winsz,
        filePath);
}

std::vector<uint8_t> Dgram::encode_ack_packet(uint32_t seq_num){
    std::vector<uint8_t>ack_packet = encode_bytes(seq_num);
    ack_packet.insert(ack_packet.begin(), static_cast<uint8_t>(ACK_FLAG));
    return ack_packet;
}

uint32_t Dgram::decode_ack_packet(std::vector<uint8_t>ack_packet){
    uint32_t seq_num = decode_bytes(std::vector<uint8_t>(ack_packet.begin() + 1, ack_packet.end()));
    return seq_num;
}

std::vector<uint8_t> Dgram::encode_fin_packet(uint32_t seq_num){
    std::vector<uint8_t>fin_packet = encode_bytes(seq_num);
    fin_packet.insert(fin_packet.begin(), static_cast<uint8_t>(FIN_FLAG));
    std::cout<<"ENCODING FIN PACKET"<<std::endl;
    return fin_packet;
}

uint32_t Dgram::decode_fin_packet(std::vector<uint8_t>fin_packet){
    uint32_t seq_num = decode_bytes(std::vector<uint8_t>(fin_packet.begin() + 1, fin_packet.end()));
    return seq_num;
}

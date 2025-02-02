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

std::vector<uint8_t> Dgram::encode_dgram(uint16_t seq_num){ // uint16_t converted to uint8_t vector 
    uint16_t seq_network_order = htons(seq_num);
    return{
        static_cast<uint8_t>(seq_network_order >> 8),
        static_cast<uint8_t>(seq_network_order & 0xFF)
    };
}

uint16_t Dgram::decode_dgram(std::vector<uint8_t> seq_num){ // uint8_t vector converted to uint16_t
    uint16_t seq_host_order = ntohs((static_cast<uint16_t>seq_num[0] << 8) | seq_num[1]);
    return seq_host_order;
}
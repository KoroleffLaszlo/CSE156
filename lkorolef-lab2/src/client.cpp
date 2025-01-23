#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdint>
#include <fstream>
#include <map>

#include "../include/client.h"
#include "../include/packet_t.h"

Client::Client() : socket_p(-1){};

Client::~Client(){
    if(socket_p >= 0) close(socket_p);
}

void Client::socket_init(){
    struct timeval timeout;
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    socket_p = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_p < 0){
        throw std::runtime_error(std::string("socket initialization failed: ") + std::string(strerror(errno)));
    }
    if(setsockopt(socket_p, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
        close(socket_p);
        throw std::runtime_error(std::string("Failed to set socket timeout: ") + std::string(strerror(errno)));
    }
    if(setsockopt(socket_p, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0){
        close(socket_p);
        throw std::runtime_error("Failed to set send timeout: " + std::string(strerror(errno)));
    }
}

size_t Client::client_send_and_receive(const int &srv_port,
                                    const char* srv_ip,
                                    const std::vector<Packet_t::dgram_t> &pack_buffer,
                                    std::ofstream &output_file,
                                    size_t write_position,
                                    const int& win_size){

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(srv_port);
    if(inet_pton(AF_INET, srv_ip, &srv_addr.sin_addr) <= 0){
        throw std::runtime_error("Invalid server IP address");
    }
    socklen_t addr_len = sizeof(srv_addr);
    std::vector<Packet_t::dgram_t> recv_packets;
    std::map<uint16_t, std::vector<uint8_t>> packet_data; //enforces proper ordering 
    size_t seq_num = 0, packet_num = 0;
    while(packet_num < pack_buffer.size()){
        for(int i = 0; i < win_size && (seq_num < pack_buffer.size()); i++){
            //send packets
            packet_data[pack_buffer[seq_num].sequence_num] = std::vector<uint8_t>(
                pack_buffer[seq_num].data_body.begin() + 2,
                pack_buffer[seq_num].data_body.end()
            );
            int bytes_sent = sendto(socket_p, 
                                pack_buffer[seq_num].data_body.data(), 
                                pack_buffer[seq_num].data_body.size(), 0,
                                (struct sockaddr*)&srv_addr, addr_len);
            if(bytes_sent < 0){
                close(socket_p);
                throw std::runtime_error(std::string("client bytes sent failed: ") + std::string(strerror(errno)));
            }
            seq_num++;
        } 
        //recv packets
        for(int i = 0; i < win_size && (packet_num < pack_buffer.size()); i++){
            std::vector<Packet_t::dgram_t> temp_buffer; 
            int bytes_recv = recvfrom(socket_p, temp_buffer.data(), temp_buffer.size(), 0, 
                                (struct sockaddr*)&srv_addr, &addr_len);
            if(bytes_recv < 0){
                close(socket_p);
                throw std::runtime_error(std::string("client bytes received failed: ") + std::string(strerror(errno)));
            }
            packet_num++;
        }
        //constant writing to output file
        for(const auto &entry : packet_data){
            output_file.seekp(write_position);
            output_file.write(reinterpret_cast<const char*>(entry.second.data()), entry.second.size());
            write_position += entry.second.size();
        }
        packet_data.clear();
    }
    return write_position;
}
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
#include <fstream>          
#include <sys/stat.h>
#include <limits.h>

#include "../include/client.h"

#define WINDOW_SIZE 6

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
                                    const std::vector<Client::dgram_t> &pack_buffer,
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
    std::vector<Client::dgram_t> recv_packets;
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
            std::vector<Client::dgram_t> temp_buffer; 
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

std::vector<uint8_t> Client::encode_dgram(uint16_t sequence_num) {
    uint16_t seq_network_order = htons(sequence_num);
    return {
        static_cast<uint8_t>(seq_network_order >> 8),
        static_cast<uint8_t>(seq_network_order & 0xFF)
    };
}

//introduce sliding window on file reading level to reduce memory usage 
void Client::fileRead(const std::string &filename, 
                    const std::string &output_filename,
                    const char* srv_ip,
                    const int& mtu_size,
                    const int& srv_port){
    
    std::fstream file_input(filename, std::ios::in | std::ios::binary);
    if(!file_input.is_open() || file_input.fail()){
        throw std::runtime_error("Failed to open input file: " + filename + " - " + std::string(strerror(errno)));
    }

    std::ofstream file_output(output_filename, std::ios::binary | std::ios::trunc);
    if(!file_output.is_open() || file_output.fail()){
        throw std::runtime_error("Failed to open input file: " + output_filename + " - " + std::string(strerror(errno)));
    }

    std::vector<uint8_t> buffer(mtu_size);
    size_t write_position = 0;
    socket_init();
    uint16_t sequence_num = 0;
    std::vector<Client::dgram_t> packets;
    while(file_input.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || file_input.gcount() > 0){
        Client::dgram_t single_packet;
        single_packet.sequence_num = sequence_num;
        std::vector<uint8_t> encode_seq = encode_dgram(sequence_num);
        single_packet.data_body.insert(single_packet.data_body.begin(), encode_seq.begin(), encode_seq.end()); //seq num at beginning
        single_packet.data_body.insert(single_packet.data_body.begin() + 2, buffer.begin(), buffer.begin() + file_input.gcount()); //binary file data

        packets.emplace_back(std::move(single_packet)); //efficient moving of a single packet to packet vector
        sequence_num++;

        if(packets.size() == WINDOW_SIZE){
            write_position = client_send_and_receive(srv_port, srv_ip, packets, file_output, write_position, WINDOW_SIZE);
            packets.clear();
        }
    }
    if(!packets.empty()){
        write_position = client_send_and_receive(srv_port, srv_ip, packets, file_output, write_position, WINDOW_SIZE);
        packets.clear();
    }
    file_input.close();
    file_output.close();
}
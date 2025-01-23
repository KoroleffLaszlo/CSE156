#include <iostream> 
#include <fstream>     
#include <vector>        
#include <string>       
#include <cstring>       
#include <cerrno>        
#include <sys/types.h>   
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>   
#include <unistd.h>    
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>

#include "../include/file_t.h"
#include "../include/packet_t.h"
#include "../include/client.h"

#define WINDOW_SIZE 6

//introduce sliding window on file reading level to reduce memory usage 
void File_t::fileRead(const std::string &filename, 
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
    struct Client client_handler;
    struct Packet_t packet_handler;
    client_handler.socket_init();
    uint16_t sequence_num = 0;
    std::vector<Packet_t::dgram_t> packets;
    while(file_input.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || file_input.gcount() > 0){
        Packet_t::dgram_t single_packet;
        single_packet.sequence_num = sequence_num;
        std::vector<uint8_t> encode_seq = packet_handler.encode_dgram(sequence_num);
        single_packet.data_body.insert(single_packet.data_body.begin(), encode_seq.begin(), encode_seq.end()); //seq num at beginning
        single_packet.data_body.insert(single_packet.data_body.begin() + 2, buffer.begin(), buffer.begin() + file_input.gcount()); //binary file data

        packets.emplace_back(std::move(single_packet)); //efficient moving of a single packet to packet vector
        sequence_num++;

        if(packets.size() == WINDOW_SIZE){
            write_position = client_handler.client_send_and_receive(srv_port, srv_ip, packets, file_output, write_position, WINDOW_SIZE);
            packets.clear();
        }
    }
    if(!packets.empty()){
        write_position = client_handler.client_send_and_receive(srv_port, srv_ip, packets, file_output, write_position, WINDOW_SIZE);
        packets.clear();
    }
    file_input.close();
    file_output.close();
}
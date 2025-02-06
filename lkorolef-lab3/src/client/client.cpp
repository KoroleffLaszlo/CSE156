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
#include <sys/stat.h>
#include <limits.h>
#include <bitset>
#include <chrono>
#include <tuple>

#include "../../include/client/client.h"
#include "../../include/common/datagram.h"
#include "../../include/common/file_t.h"

#define MTU_MAX 32000

#define META_FLAG 0
#define DATA_FLAG 1
#define ACK_FLAG 2
#define FIN_FLAG 3 // terminating packet

constexpr int TIMEOUT_MS = 100; // 100ms timeout for select() 

Dgram dgram;
File file_handle;

std::map<uint32_t, std::pair<std::vector<uint8_t>, std::chrono::steady_clock::time_point>> packet_window;

Client::Client() : socket_p(-1){};

Client::~Client(){
    if(socket_p >= 0) close(socket_p);
}

void Client::socket_init(){
    struct timeval timeout;
    timeout.tv_sec = 30; // client times out after max 30 seconds
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

void Client::client_send_packet(struct sockaddr_in &srv_addr, 
                                int p_flag,
                                uint32_t seq_num, 
                                std::vector<uint8_t> data, 
                                const socklen_t &addr_len){
    if(p_flag == META_FLAG || p_flag == FIN_FLAG){
        sendto(socket_p, data.data(), data.size(), 0, (struct sockaddr*)&srv_addr, addr_len);
        packet_window[seq_num] = {data, std::chrono::steady_clock::now()};
    }else{ //DATA_FLAG
        std::vector<uint8_t> packet = dgram.encode_data_packet(seq_num, data);
        std::cout<<"Sent Data - Sequence Number : "<< seq_num << std::endl;
        sendto(socket_p, packet.data(), packet.size(), 0, (struct sockaddr*)&srv_addr, addr_len);
        packet_window[seq_num] = {packet, std::chrono::steady_clock::now()};
    }
    return;
}

// void Client::client_send_data(struct sockaddr_in &srv_addr, uint32_t seq_num, std::vector<uint8_t> data, const socklen_t &addr_len){
//     std::vector<uint8_t> packet = dgram.encode_data_packet(seq_num, data);
//     std::cout<<"Sent Data - Sequence Number : "<< seq_num << std::endl;
//     sendto(socket_p, packet.data(), packet.size(), 0, (struct sockaddr*)&srv_addr, addr_len);
//     packet_window[seq_num] = {packet, std::chrono::steady_clock::now()};
//     return;
// }

// void Client::set_client_info(struct sockaddr_in &srv_addr, uint32_t seq_num, std::vector<uint8_t> packet, const socklen_t &addr_len){
//     sendto(socket_p, packet.data(), packet.size(), 0, (struct sockaddr*)&srv_addr, addr_len);
//     packet_window[seq_num] = {packet, std::chrono::steady_clock::now()};
//     return;
// }

void Client::waitForAck(struct sockaddr_in &srv_addr, socklen_t addr_len){
    fd_set read_fds;
    struct timeval select_timeout;
    std::map<uint32_t, int> retransmission_count;  // Track retransmission attempts

    while(!packet_window.empty()){  // Exit only when all packets are acknowledged
        FD_ZERO(&read_fds);
        FD_SET(socket_p, &read_fds);

        select_timeout.tv_sec = 0;
        select_timeout.tv_usec = 50 * 1000;  // 50ms timeout for checking ACKs

        int active = select(socket_p + 1, &read_fds, nullptr, nullptr, &select_timeout);

        if(active > 0 && FD_ISSET(socket_p, &read_fds)){
            std::vector<uint8_t> ack_buffer(MTU_MAX);
            int bytes_received = recvfrom(socket_p, ack_buffer.data(), ack_buffer.size(), 0, (struct sockaddr*)&srv_addr, &addr_len);
            ack_buffer.resize(bytes_received);

            uint32_t sequence_num = dgram.decode_ack_packet(ack_buffer);

            if(packet_window.count(sequence_num)){
                std::cout << "Received ACK for packet - " << sequence_num << std::endl;
                packet_window.erase(sequence_num);
                retransmission_count.erase(sequence_num);
            }
        }

        // retransmit unACKed packets
        auto now = std::chrono::steady_clock::now();
        for (auto it = packet_window.begin(); it != packet_window.end();) {
            uint32_t seq_num = it->first;
            auto &packet_info = it->second;

            if (now - packet_info.second > std::chrono::milliseconds(TIMEOUT_MS)) {
                retransmission_count[seq_num]++;

                if (retransmission_count[seq_num] > 5) {
                    std::cerr << "Packet " << seq_num << " lost after 5 retransmissions. Exiting with return code 4." << std::endl;
                    exit(4);
                }
                // MAKE SURE DATA CONTAINS FLAGS AND SEQUENCE NUMBERS
                sendto(socket_p, packet_info.first.data(), packet_info.first.size(), 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                std::cout << "Retransmitted Packet " << seq_num << " (Attempt " << retransmission_count[seq_num] << "/5)" << std::endl;

                packet_info.second = now;
            }
            ++it;
        }
    }
}

// sends winsz number of packets of data size mss, receives ACK
void Client::client_communicate(const char *srv_ip, 
                        const int &srv_port, 
                        const uint32_t &winsz, 
                        const int &mss, 
                        const std::string &input_file,
                        const std::string &output_file){
    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(srv_port);
    if(inet_pton(AF_INET, srv_ip, &srv_addr.sin_addr) <= 0){
        close(socket_p);
        throw std::runtime_error("Invalid server IP address");
    }
    socklen_t addr_len = sizeof(srv_addr);

    uint32_t seq_num = 0;

    // send metadata packet
    std::vector<uint8_t> meta_packet = dgram.encode_meta_packet(winsz, output_file);
    client_send_packet(srv_addr, META_FLAG, seq_num, meta_packet, addr_len);
    waitForAck(srv_addr, seq_num);
    seq_num++;
    int read_pos = 0;

    // send data packets
    while(true){
        packet_window.clear();  // Clear previous batch
        for (uint32_t i = 0; i < winsz; i++) {
            std::pair<std::vector<uint8_t>, int> file_read_ret = file_handle.file_read_stream(input_file, mss, read_pos);
            std::vector<uint8_t>data_chunk = file_read_ret.first;
            std::cout<<"seq_num: "<<seq_num << "Chunk: ";
            std::cout<<"[";
            for(size_t i = 0; i < data_chunk.size(); i++){
                std::cout<<(char)data_chunk[i];
            }
            std::cout<<"]"<<std::endl;
            if (data_chunk.size() == 0){
                std::cout<<"no data to read"<<std::endl;
                read_pos = -1;
                break;
            }
            read_pos += file_read_ret.second;
            client_send_packet(srv_addr, DATA_FLAG, seq_num, data_chunk, addr_len);
            sleep(3);
            seq_num++;
        }
        waitForAck(srv_addr, addr_len);  // Wait for all ACKs before sending the next batch
        if (packet_window.empty() && (read_pos < 0)){
            break;
        }
    }

    // **Step 3: Send FIN_FLAG Packet and Wait for ACK**
    seq_num++;
    std::vector<uint8_t> fin_payload = dgram.encode_fin_packet(seq_num);
    client_send_packet(srv_addr, FIN_FLAG, seq_num, fin_payload, addr_len);
    waitForAck(srv_addr, addr_len);
    return;
}
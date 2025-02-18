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
#define FIN_FLAG 3
#define TERM_FLAG 4 // termination flag client trying to write to already processed file

Dgram dgram;
File file_handle;

std::map<uint32_t, std::vector<uint8_t>> packet_window; // Stores packets without timestamps

Client::Client() : socket_p(-1){};

Client::~Client(){
    if(socket_p >= 0) close(socket_p);
}

uint32_t base_seq = 0;
uint32_t next_seq = 0;
int win = 0;

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

void Client::logPacketEvent(const std::string& type, uint32_t pkt_sn){
    // Get current timestamp in RFC 3339 format
    std::time_t now = std::time(nullptr);
    std::tm* utcTime = std::gmtime(&now); 

    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000Z", utcTime);
    std::string timestamp(buffer);

    // Prepare log entry
    std::string log_entry = timestamp + ", " + type + ", " + std::to_string(pkt_sn) + ", " +
                            std::to_string(base_seq) + ", " + std::to_string(next_seq) + ", " +
                            std::to_string(base_seq + win) + "\n";

    std::cout << log_entry;
}

void Client::client_send_packet(struct sockaddr_in &srv_addr, 
                                int p_flag,
                                uint32_t seq_num, 
                                std::vector<uint8_t> data, 
                                const socklen_t &addr_len) {
    
    if (p_flag == META_FLAG || p_flag == FIN_FLAG) {
        sendto(socket_p, data.data(), data.size(), 0, (struct sockaddr*)&srv_addr, addr_len);
        packet_window[seq_num] = data;  // Store only packet data (no timer)
    } else {  // DATA_FLAG
        std::vector<uint8_t> packet = dgram.encode_data_packet(seq_num, data);
        sendto(socket_p, packet.data(), packet.size(), 0, (struct sockaddr*)&srv_addr, addr_len);
        packet_window[seq_num] = packet;  // Store only packet data (no timer)
    }
    logPacketEvent("DATA", seq_num);
    return;
}

// used specifically for META/FIN packets only
void Client::waitForAck(struct sockaddr_in &srv_addr, socklen_t addr_len) {
    std::map<uint32_t, int> retransmission_count;  // Track retransmission attempts

    while (!packet_window.empty()) {  // Exit only when all packets are acknowledged
        std::vector<uint8_t> ack_buffer(MTU_MAX);
        int bytes_received = recvfrom(socket_p, ack_buffer.data(), ack_buffer.size(), 0, 
                                      (struct sockaddr*)&srv_addr, &addr_len);

        if (bytes_received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                std::cerr << "Timeout: No response from server for 30 seconds. Exiting with return code 3." << std::endl;
                exit(3);
            } else {
                std::cerr << "Error receiving ACK/TRANS packet: " << strerror(errno) << std::endl;
                exit(3);
            }
        }

        ack_buffer.resize(bytes_received);
        uint8_t flag = ack_buffer[0];  // First byte is flag
        uint32_t sequence_num = dgram.decode_ack_packet(ack_buffer);

        if(flag == ACK_FLAG){
            if (packet_window.count(sequence_num)) {
                packet_window.erase(sequence_num);
                retransmission_count.erase(sequence_num);
                logPacketEvent("ACK", sequence_num);
            }
        } 
        else if(flag == TERM_FLAG){
            if (packet_window.count(sequence_num)) {
                retransmission_count[sequence_num]++;
                if (retransmission_count[sequence_num] > 5) {
                    //std::cerr << "Packet " << sequence_num << " lost after 5 retransmissions. Exiting with return code 4." << std::endl;
                    exit(4);
                }
                // Retrieve packet data and resend
                auto &packet_data = packet_window[sequence_num];
                sendto(socket_p, packet_data.data(), packet_data.size(), 0, 
                       (struct sockaddr*)&srv_addr, sizeof(srv_addr));
            }
        }
    }
}

// 
void Client::client_communicate(const char *srv_ip, 
                                const int &srv_port, 
                                const uint32_t &winsz, 
                                const int &mss, 
                                const std::string &input_file,
                                const std::string &output_file) {
    win = winsz;
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
    base_seq = 0;
    next_seq = seq_num + 1;

    struct timeval recv_timeout = {30, 0};  
    if (setsockopt(socket_p, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0) {
        std::cerr << "Error setting recv timeout: " << strerror(errno) << std::endl;
        exit(1);
    }

    // sendto timeout 3sec
    struct timeval send_timeout = {3, 0};  
    if (setsockopt(socket_p, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout)) < 0) {
        std::cerr << "Error setting send timeout: " << strerror(errno) << std::endl;
        exit(1);
    }

    // metadata pack
    std::vector<uint8_t> meta_packet = dgram.encode_meta_packet(winsz, output_file);
    client_send_packet(srv_addr, META_FLAG, seq_num, meta_packet, addr_len);
    waitForAck(srv_addr, addr_len);
    seq_num++;
    next_seq++;
    int read_pos = 0;

    // send data packets
    while (true) {
        packet_window.clear(); 
        for(uint32_t i = 0; i < winsz; i++){
            std::pair<std::vector<uint8_t>, int> file_read_ret = file_handle.file_read_stream(input_file, mss, read_pos);
            std::vector<uint8_t> data_chunk = file_read_ret.first;
            if(data_chunk.empty()){
                read_pos = -1;
                break;
            }
            read_pos += file_read_ret.second;

            // Store packet in `packet_window`
            packet_window[seq_num] = data_chunk;

            // Send the packet
            client_send_packet(srv_addr, DATA_FLAG, seq_num, data_chunk, addr_len);
            //sleep(5); // debugging
            seq_num++;
            next_seq++;
        }

        // Wait for ACKs before sending the next batch
        waitForAck(srv_addr, addr_len);
        base_seq = base_seq + next_seq;
        // Exit when all packets are acknowledged and EOF is reached
        if(packet_window.empty() && (read_pos < 0)){
            break;
        }
    }

    // send FIN packet
    seq_num++;
    std::vector<uint8_t> fin_payload = dgram.encode_fin_packet(seq_num);
    client_send_packet(srv_addr, FIN_FLAG, seq_num, fin_payload, addr_len);
    waitForAck(srv_addr, addr_len);
}
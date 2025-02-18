#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <map>
#include <utility>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/types.h>   
#include <sys/time.h>    
#include <unistd.h>      
#include <fcntl.h>     
#include <bitset>
#include <iomanip>
#include <ctime>

#include "../../include/server/server.h"
#include "../../include/server/conn.h"
#include "../../include/common/datagram.h"
#include "../../include/common/file_t.h"

#define MTU_MAX 32000
#define TIMEOUT 3

#define META_FLAG 0
#define FIN_FLAG 3 // client finished terminate connection
#define TERM_FLAG 4

Conn conn;
Dgram dgram;
File file_handle;

#define MTU_MAX 32000
Server::Server() : socket_p(-1){};

Server::~Server(){
    if(socket_p >= 0) close(socket_p);
}

void Server::logPacketEvent(const std::string &type, uint32_t pkt_sn){
    std::time_t now = std::time(nullptr);
    std::tm* utcTime = std::gmtime(&now);
    
    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000Z", utcTime);
    std::string timestamp(buffer);

    // Print log entry
    std::cout << timestamp << ", " << type << ", " << pkt_sn << std::endl;
}

double drop_rate;
int total_packets_processed = 0;
int dropped_count = 0;

bool Server::should_drop(){
    total_packets_processed++;
    double current_drop_rate = (double)dropped_count / total_packets_processed;
    
    if(current_drop_rate < drop_rate){
        dropped_count++;
        return true;
    }
    return false;
}

void Server::socket_init(){
    socket_p = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_p < 0){
        close(socket_p);
        throw std::runtime_error(std::string("server socket initialization failed: ") + std::string(strerror(errno)));
    }
}

void Server::server_bind(struct sockaddr_in &srv_addr, const int& port){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    if((bind(socket_p, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0)){
        close(socket_p);
        throw std::runtime_error(std::string("server bind failed: ") + std::string(strerror(errno)));
    }
}

void Server::termination(struct sockaddr_in &client_addr, uint32_t seq_num, const socklen_t &addr_len){
    std::vector<uint8_t> data_body = dgram.encode_bytes(seq_num);
    data_body.insert(data_body.begin(), TERM_FLAG);
    int bytes_sent = sendto(socket_p, data_body.data(), data_body.size(), 0, (struct sockaddr*)&client_addr, addr_len);
    if(bytes_sent < 0){
        close(socket_p);
        throw std::runtime_error(std::string("Server bytes sent failed: ") + std::string(strerror(errno)));
    }
    return;
}

void Server::server_send_ack(struct sockaddr_in &client_addr, uint32_t seq_num, const socklen_t &addr_len, int droppc){
    if(should_drop()){
        logPacketEvent("DROP ACK", seq_num);
        retransmission(client_addr, seq_num, addr_len);
        return;
    }
    std::vector<uint8_t> ack_packet = dgram.encode_ack_packet(seq_num);
    int bytes_sent = sendto(socket_p, ack_packet.data(), ack_packet.size(), 0, (struct sockaddr*)&client_addr, addr_len);
    if(bytes_sent < 0){
        close(socket_p);
        throw std::runtime_error(std::string("Server bytes sent failed: ") + std::string(strerror(errno)));
    }
    logPacketEvent("ACK", seq_num);
    return;
}

std::vector<std::string> file_history;

void Server::server_recv(const int& droppc){
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t addr_len = (socklen_t)sizeof(client_addr);
    drop_rate = droppc / 100.0;
    
    while(1){
        std::vector<uint8_t> buffer(MTU_MAX);
        memset(&client_addr, 0, sizeof(client_addr));
        int bytes_read = recvfrom(socket_p, buffer.data(), buffer.size(), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (bytes_read < 0){
            close(socket_p);
            throw std::runtime_error(std::string("Server bytes received failed: ") + std::string(strerror(errno)));
        }
        buffer.resize(bytes_read);

        std::string client_ip = inet_ntoa(client_addr.sin_addr);
        uint16_t client_port = ntohs(client_addr.sin_port);

        // creating Client profile (clientState struct) for new/existing client
        if(buffer[0] == META_FLAG){
            if(should_drop()){
                logPacketEvent("DROP DATA", static_cast<uint32_t>(0));
                continue;
            }
            std::pair<uint32_t, std::string> meta_data = dgram.decode_meta_packet(buffer);
            if(file_history.find(meta_data.second)){ // if file is already being written to from other client
                std::cerr<<"ERROR CLIENT WRITING TO FILE: "<< meta_data.second<< ":" << client_ip << " - " << client_port<<std::endl;
                termination(client_addr, 0, addr_len);
            }
            conn.addClient(client_ip, client_port, meta_data.second, meta_data.first);
            server_send_ack(client_addr, 0, addr_len, droppc); // send ack for meta (sequence num is 0)
            Conn::ClientState& clientState = conn.getClientState(client_ip, client_port);
            clientState.expected_seq_num += 1; //starting from 1 (META always 0)
            continue;
        }
        
        //data packets: [0]:flag; [1-4]:seq_num; [5-end]: data-body
        Conn::ClientState& clientState = conn.getClientState(client_ip, client_port);
        uint32_t seq_num = dgram.decode_bytes(std::vector<uint8_t>(buffer.begin() + 1, buffer.begin() + 5));
        
        if(should_drop()){
            logPacketEvent("DROP DATA", seq_num);
            continue;
        }

        logPacketEvent("DATA", seq_num);

        if(buffer[0] == FIN_FLAG){
            uint32_t fin_seq_num = dgram.decode_fin_packet(buffer);
            std::mutex file_mutex = conn.getFileMutex(clientState.filePath);
            server_send_ack(client_addr, fin_seq_num, addr_len, droppc);
            int check = file_handle.file_write_stream(clientState.filePath, clientState.buffer, file_mutex);
            if(check < 0){
                close(socket_p);
                throw std::runtime_error(std::string("writing errors: ") + std::string(strerror(errno)));
            }
            file_name = conn.getClientFile(client_ip, client_port);
            file_history.erase(file_history.find(file_name));
            conn.removeClient(client_ip, client_port);
            continue;
        }
        
        // ACK lost in response to client, don't duplicate -> resend ACK
        if(clientState.buffer.find(seq_num) != clientState.buffer.end()){ //if exists
            server_send_ack(client_addr, seq_num, addr_len, droppc);
        }else{
            clientState.buffer[seq_num] = std::vector<uint8_t>(buffer.begin() + 5, buffer.end());
            clientState.expected_seq_num += 1; //starting from 1 (META always 0)
            server_send_ack(client_addr, seq_num, addr_len, droppc);
        }
        
        if(clientState.buffer.size() == clientState.winSize){
            // print window and clear 
            int check = file_handle.file_write_stream(clientState.filePath, clientState.buffer, file_mutex);
            if(check < 0){
                close(socket_p);
                throw std::runtime_error(std::string("writing errors: ") + std::string(strerror(errno)));
            }
            clientState.buffer.erase(clientState.buffer.begin(), clientState.buffer.end());
        }
    }
}
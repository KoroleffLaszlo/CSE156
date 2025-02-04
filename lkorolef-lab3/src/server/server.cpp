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

#include "../../include/server/server.h"
#include "../../include/server/conn.h"
#include "../../include/common/datagram.h"

#define TIMEOUT 3
#define META_FLAG 0
#define TERM_FLAG 4 // client finished terminate connection

Conn conn;
Dgram dgram;

#define MTU_MAX 32000
Server::Server() : socket_p(-1){};

Server::~Server(){
    if(socket_p >= 0) close(socket_p);
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

void Server::server_send_ack(struct sockaddr_in &client_addr, uint16_t seq_num, const socklen_t &addr_len){
    std::vector<uint8_t> ack_packet = dgram.encode_ack_packet(seq_num);
    int bytes_sent = sendto(socket_p, ack_packet.data(), ack_packet.size(), 0, (struct sockaddr*)&client_addr, addr_len);
    if(bytes_sent < 0){
        close(socket_p);
        throw std::runtime_error(std::string("Server bytes sent failed: ") + std::string(strerror(errno)));
    }
}

void Server::server_recv(const int& droppc){
    struct sockaddr_in client_addr;
    fd_set rfds; //setting timeout for multi-client handling
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t addr_len = (socklen_t)sizeof(client_addr);

    while(1){
        FD_ZERO(&rfds);
        FD_SET(socket_p, &rfds);
        struct timeval timeout = {TIMEOUT,0};

        int err = select(socket_p + 1, &rfds, NULL, NULL, &timeout);
        if(err == -1){
            std::cerr<<"Connection lost to client - Error: "<< strerror(errno) << std::endl;
            continue; // client lost connection so connect to possible other clients
        }
        if(FD_ISSET(socket_p, &rfds)){ // connected to single client
            std::vector<uint8_t> buffer(MTU_MAX);
            memset(&client_addr, 0, sizeof(client_addr));
            int bytes_read = recvfrom(socket_p, buffer.data(), buffer.size(), 0, (struct sockaddr*)&client_addr, &addr_len);
            if (bytes_read < 0){
                close(socket_p);
                throw std::runtime_error(std::string("Server bytes received failed: ") + std::string(strerror(errno)));
            }
            buffer.resize(bytes_read);

            // simulate packet dropping
            int rand_num = rand() % 100;
            if(rand_num < droppc){ // TODO: Log dropped packets 
                continue;
            }

            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            uint16_t client_port = ntohs(client_addr.sin_port);

            // creating Client profile (clientState struct) for new/existing client
            if(buffer[0] == META_FLAG){
                std::pair<uint16_t, std::string> meta_data = dgram.decode_meta_packet(buffer);
                conn.addClient(client_ip, client_port, meta_data.second, meta_data.first);
                continue;
            }

            //else data packet
            //data packets: [0]:flag; [1-2]:seq_num; [3-end]: data-body
            uint16_t seq_num = dgram.decode_bytes(std::vector<uint8_t>(buffer.begin() + 1, buffer.begin() + 3));
            Conn::ClientState& clientState = conn.getClientState(client_ip, client_port);

            // ACK lost in response to client, don't duplicate -> resend ACK
            if(clientState.buffer.find(seq_num) != clientState.buffer.end()){
                server_send_ack(client_addr, seq_num, addr_len);
            }else{
                // else store into clientState map and send ACK
                clientState.buffer[seq_num] = std::vector<uint8_t>(&buffer[3], &buffer[bytes_read - 3]);
                server_send_ack(client_addr, seq_num, addr_len);
            }
            if(clientState.buffer.size() == clientState.winSize){
                //TODO: write file function
                clientState.buffer.clear();
                continue; // remove once writing function works
            }
        }
    }
}
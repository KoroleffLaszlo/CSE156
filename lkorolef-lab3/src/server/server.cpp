#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/types.h>   
#include <sys/time.h>    
#include <unistd.h>      
#include <fcntl.h>     

#include "../include/server.h"
#include "../include/conn.h"

#define META_FLAG 0
#define ACK_FLAG 2

Conn conn;

#define MTU_MAX 32000
Server::Server() : socket_p(-1){};

Server::~Server(){
    if(socket_p >= 0) close(socket_p);
}

void Server::socket_init(){
    socket_p = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_p < 0){
        throw std::runtime_error(std::string("server socket initialization failed: ") + std::string(strerror(errno)));
    }
}

void Server::server_bind(struct sockaddr_in &srv_addr, const int& port){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    if((bind(socket_p, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0)){
        throw std::runtime_error(std::string("server bind failed: ") + std::string(strerror(errno)));
    }
}

void Server::server_send_ack(struct sockaddr_in &client_addr, const std::vector<uint8_t> &buffer, const socklen_t &addr_len){
    int bytes_sent = sendto(socket_p, buffer.data(), buffer.size(), 0, (struct sockaddr*)&client_addr, addr_len);
    if(bytes_sent < 0){
        throw std::runtime_error(std::string("server bytes sent failed: ") + std::string(strerror(errno)));
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
        struct timeval timeout = {1,0};

        err = select(socket_p + 1, &rfds, NULL, NULL, &timeout);
        if(err == -1){
            std::cerr<<"Connection lost to client - Error: "<< strerror(errno) <<std::end;
            continue; // client lost connection so connect to possible other clients or "listen"
        }

        if(FD_ISSET(socket_p, &rfds)){ // connected to single client 
            std::vector<uint8_t> buffer(MTU_MAX);
            memset(&client_addr, 0, sizeof(client_addr));
            int bytes_read = recvfrom(socket_p, buffer.data(), buffer.size(), 0, (struct sockaddr*)&client_addr, &addr_len);
            if (bytes_read < 0){
                throw std::runtime_error(std::string("server bytes received failed: ") + std::string(strerror(errno)));
            }
            buffer.resize(bytes_read);
            // simulate packet dropping
            int rand_num = rand() % 100;
            if(rand_num < droppc){
                continue;
            }

            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            uint16_t client_port = ntohs(client_addr.sin_port);

            // creating Client profile (clientState struct) for possible new client
            if(buffer[0] == META_FLAG){ // metadata packet 
                std::string filePath = reinterpret_cast<char*>(&buffer[1]);
                if(!conn.clientExists(client_ip, client_port)){
                    conn.addClient(client_ip, client_port, filePath, &buffer[3]); //winsize : 3rd index of metadata 
                }
                continue;
            }
            //else data packet
            uint16_t seq_num;
            memcpy(&seq_num, &buffer[1], sizeof(seq_num)); // two bytes seq_num -> buffer[1-2]
            Conn::ClientState *clientState = conn.getClientState(client_ip, client_port);

            // ACK lost in response to client, don't duplicate
            if(clientState->buffer.find(seq_num) != clientState->buffer.end()){
                server_send_ack(client_addr, std::vector<uint8_t>{ACK_FLAG, seq_num}, addr_len);
            }else{
                //else store into clientState map 
                clientState->buffer[seq_num] = std::vector<uint8_t>(&buffer[3], &buffer[bytes_read]);
            }
            if(clientState->buffer.size() == clientState->winSize){
                write_file(); //TODO: write file function 
                clientState->buffer.clear();
            }
        }
    }
}
#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "../include/server.h"

#define WINDOW_SIZE 6
#define MTU_MAX 32000
Server::Server() : socket_p(-1){};

Server::~Server(){
    if(socket_p >= 0) close(socket_p);
}

void Server::socket_init(){
    socket_p = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_p < 0){
        throw std::runtime_error(std::string("socket initialization failed: ") + std::string(strerror(errno)));
    }
}

void Server::server_bind(struct sockaddr_in &srv_addr, const int port){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    if((bind(socket_p, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0)){
        throw std::runtime_error(std::string("server bind failed: ") + std::string(strerror(errno)));
    }
}

void Server::server_send(struct sockaddr_in &client_addr, const std::vector<uint8_t> &buffer, const socklen_t &addr_len){
    int bytes_sent = sendto(socket_p, buffer.data(), buffer.size(), 0, (struct sockaddr*)&client_addr, addr_len);
    if(bytes_sent < 0){
        throw std::runtime_error(std::string("server bytes sent failed: ") + std::string(strerror(errno)));
    }
}

void Server::server_recv(){
    while(1){
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        socklen_t addr_len = (socklen_t)sizeof(client_addr);
        std::vector<uint8_t> buffer(MTU_MAX);
        int bytes_read = recvfrom(socket_p, buffer.data(), buffer.size(), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (bytes_read < 0){
            throw std::runtime_error(std::string("server bytes received failed: ") + std::string(strerror(errno)));
        }
        buffer.resize(bytes_read); //downsizes vector to client set MTU size or smaller
        server_send(client_addr, buffer, addr_len);
    }
}
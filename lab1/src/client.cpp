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


#include "../include/client.h"
#define BUFFER_SIZE 2048

Client::Client() : socket_p(-1){};

Client::~Client(){
    if(socket_p >= 0) close(socket_p);
}

void Client::socket_init(){
    int socket_p = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_p < 0){
        throw std::runtime_error(std::string("socket_init() failed: ") + std::string(strerror(errno)));
    }

    return;
}

void Client::connectToServer(struct sockaddr_in &srv_addr, const std::string& ip, uint16_t port){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    if(inet_pton(AF_INET, ip.c_str(), &srv_addr.sin_addr) < 0){
        close(socket_p);
        throw std::runtime_error("Invalid address: " + std::string(strerror(errno)));
    }

    if(connect(socket_p, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0){
        close(socket_p);
        throw std::runtime_error("Connection failure: " + std::string(strerror(errno)));
        return;
    }

    return;
}

void Client::sendToServer(const std::string &request){
    if(send(socket_p, request.c_str(), request.length(), 0) < 0){
        close(socket_p);
        throw std::runtime_error("Request failure: " + std::string(strerror(errno)));
    }

    return;
}

void Client::recieveData(std::vector<char>& buffer){
    char temp_buffer[BUFFER_SIZE] ;
    int err;
    while((err = recv(socket_p, temp_buffer, BUFFER_SIZE, 0)) > 0){
        buffer.insert(buffer.end(), temp_buffer, temp_buffer + err);
    }

    if(err < 0){
        close(socket_p );
        throw std::runtime_error("Download failure: " + std::string(strerror(errno)));
    }

    return;
}
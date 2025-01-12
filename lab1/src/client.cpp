#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "../include/client.h"
#define BUFFER_SIZE 2048

int Client::socket_init(){
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if(soc < 0){
        perror("socket_init() Error");
        std::cerr << "Error message: " << strerror(errno) << std::endl;
        return -1;
    }

    return soc;
}

void Client::connectToServer(int soc, struct sockaddr_in &srv_addr, std::string ip, uint16_t port){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    if(inet_pton(AF_INET, ip.c_str(), &srv_addr.sin_addr) < 0){
        perror("Invalid address");
        std::cerr << "Error message: " << strerror(errno) << std::endl;
        close(soc);
        return;
    }

    if(connect(soc, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0){
        perror("Connection to server Error");
        std::cerr << "Error message: " << strerror(errno) << std::endl;
        close(soc);
        return;
    }

    return;
}

void sendToServer(int soc, std::string request){
    if(send(soc, request.c_str(), request.length(), 0) < 0){
        perror("get_request() Error");
        std::cerr << "Error message: " << strerror(errno) << std::endl;
        close(soc);
        return;
    }

    return;
}

void Client::recieveData(int soc, std::vector<char>& buffer){
    char temp_buffer[BUFFER_SIZE] ;
    int err;
    while((err = recv(soc, temp_buffer, BUFFER_SIZE, 0)) > 0){
        buffer.insert(buffer.end(), temp_buffer, temp_buffer + err);
    }

    if(err < 0){
        perror("recv() Error");
        std::cerr << "Error messge: " << strerror(errno) << std::endl;
        close(soc);
        return;
    }

    return;
}



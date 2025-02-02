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
#include <errno.h>       

#include "../include/server.h"
#include "../include/conn.h"

#define META_FLAG 0

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
    fd_set rfds;
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
            buffer.resize(bytes_read); // downsizes vector to client set MTU size or smaller

            // simulate packet dropping
            int rand_num = rand() % 100;
            if(rand_num < droppc){
                continue;
            }

            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            uint16_t client_port = ntohs(client_addr.sin_port);

            // creating Client profile (clientState strucut) for possible new client
            if(buffer[0] == META_FLAG){ // handle metadata
                std::string filePath = reinterpret_cast<(char*)
                if(!conn.clientExists(client_ip, client_port)){
                    conn.addClient(client_ip, client_port, filePath, 3); //TODO: winsize adjust "3"
                }
                continue;
            }


            uint16_t seq_num;
            memcpy(&seq_num, &buffer[1], sizeof(seq_num)); // first two bytes seq_num
            
            Conn::ClientState *clientState = conn.getClientState(client_ip, client_port);
            //store sequence number into map<> buffer

            //see if winsize has been met
                //if no -> wait 
                //else -> write content of map to outfile 
                    // clear receivedPackets and map<> buffer
            //continue back to top for more packets/clients waiting

        }
        



        server_send(client_addr, buffer, addr_len);
    }
}
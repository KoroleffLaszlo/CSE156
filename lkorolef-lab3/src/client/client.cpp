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

#include "../include/client.h"
#include "../include/datagram.h"

#define META_FLAG 0
#define DATA_FLAG 1
#define ACK_FLAG 2

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

void Client::send_metaData_packet(const sockaddr_in& srv_addr, const int &winsz){
    //send 
}

// sends winsz number of packets of data size mss, receives ACK
void Client::client_send_and_receive(const char *srv_ip, const int &srv_port, const int &winsz, const int &mss){
    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    stv_addr.sin_port = htons(srv_port);
    if(inet_pton(AF_INET, srv_ip, &srv_addr.sin_addr) <= 0){
        throw std::runtime_error("Invalid server IP address");
    }
    socklen_t addr_len = sizeof(srv_addr);

    //send metadata packet
    //read from file in chunks of winsz (pass winsz and mss to file_read())
        //save last writing spot to continue from in future?
    //send winsz number of packets to server 
        //map for each window [seq_num]->ACK key/value pairing
        //wait for ACKs after some time
            //resend packets if ACK not received
        //continue back to file_read() with saved writing spot
    // map containing winsz pckts [seq_num]->ACK key/value pairing
    
    std::map<uint16_t, uint8_t> window;
    size_t seq_num = 0, packet_num = 0;
    while(packet_num < pack_buffer.size()){
        
    }
}
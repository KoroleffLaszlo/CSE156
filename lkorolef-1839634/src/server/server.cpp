#include "../../include/server/server.h"
#include "../../include/common/datagram.h"

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
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <shared_mutex>

#define MTU_MAX 32000
#define TIMEOUT_S 1

#define MAX_CONNECTIONS 100

// namespace Proccess {

// }

Server::Server() : socket_p(-1){};

Server::~Server(){
    if(socket_p >= 0) close(socket_p);
}

void Server::socket_init(){
    socket_p = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_p < 0){
        close(socket_p);
        throw std::runtime_error(std::string("server socket initialization failed: ") 
                + std::string(strerror(errno)));
    }
}

void Server::server_bind(struct sockaddr_in &srv_addr, const int& port){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    if((setsockopt(socket_p, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) < 0){
        close(socket_p);
        throw std::runtime_error(std::string("server SO_REAUSEADDR flag failed: ") + std::string(strerror(errno)));
    }

    if((bind(socket_p, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0)){
        close(socket_p);
        throw std::runtime_error(std::string("server bind failed: ") + std::string(strerror(errno)));
    }
}

void Server::_listen(int maxSize){
    if(listen(socket_p, maxSize) < 0){
        close(socket_p);
        throw std::runtime_error(std::string("server listen failed: ") + std::string(strerror(errno)));
    }
}

std::unique_ptr<Server::Connection> Server::accept_client(){
    struct sockaddr_in client;
    socklen_t client_size = sizeof(client);
    int fd = accept(socket_p, (struct sockaddr*)&client, &client_size);
    if(fd < 0) {
        if(errno == EINTR){ // signal interupt handling (blocking handling)
            std::cout<<"[ERROR] Signal interupt detected"<<std::endl;
        }else{
            std::cerr<<"[FATAL] Failed to connect to client: "<< strerror(errno) << std::endl;
        }
        return nullptr;
    }

     // thread safe conversion from bytes to char
     char ip_str[INET_ADDRSTRLEN];
     inet_ntop(AF_INET, &client.sin_addr, ip_str, INET_ADDRSTRLEN);
 
     std::cout<<"[INFO] Client connection accepted ";
     std::cout<<"- using fd: "<<fd<< std::endl;

     return std::make_unique<Connection>(fd, std::string(ip_str));
}

void Server::server_run(std::unique_ptr<Connection> t){

    client_count++;

    char buffer[MTU_MAX];
    int client_socket = t->client_fd;

    // handling possible chunked client request
    while(t->request.find("\r\n\r\n") == std::string::npos){ // handles chunking
        int bytes_recv = recv(client_socket, buffer, sizeof(buffer), 0);
        t->request.append(buffer, bytes_recv);
    }

    std::cout<<t->request<<std::endl;
    std::pair<std::string, std::string> p = Dgram::decode_request(t->request);
    // TODO: proper logging and response back to client
    if(fsites.find(p.second) != fsites.end()){
        std::cout<<"[INFO] Requested side forbidden"<<std::endl;
    }

    client_count--;
    return;
}
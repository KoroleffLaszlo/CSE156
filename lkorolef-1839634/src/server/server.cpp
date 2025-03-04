#include "../../include/server/server.h"
#include "../../include/common/epoll_wrap.h"
#include "../../include/common/file_t.h"

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

#define MTU_MAX 32000
#define TIMEOUT_S 1

#define MAX_CONNECTIONS 100

struct timeval timeout;
timeout.tv_sec = TIMEOUT_S;
timeout.tv_usec = 0;

Server::Server() : socket_p(-1){};

Server::~Server(){
    if(socket_p >= 0) close(socket_p);
}

namespace Proxy {
    void _listen(int maxSize){
        if(listen(socket_p, maxSize) < 0){
            close(socket_p);
            throw std::runtime_error(std::string("server listen failed: ") + std::string(strerror(errno)));
        }
    }

    int _set_nonblocking(int sock){
        int flags = fcntl(sock, F_GETFL, 0);
        if(flags == -1){
            throw std::runtime_error("fcntl() get flags failed: " + std::string(strerror(errno)));
        }
        if(fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1){
            throw std::runtime_error("fcntl() set non-blocking failed: " + std::string(strerror(errno)));
        }
        return 0; 
    }

    // receives all http requests/responses
    void _http_recv(int socket_fd){
        vector<char> buffer(MUT_MAX);
        while(true){
            int bytes_read = recv(socket_fd, buffer.data(), buffer.size(), MSG_DONTWAIT);

            if(bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)){

                return;
            }

        }
        char buffer[BUFFER_SIZE];
        int bytes_read = recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
else if (bytes_read <= 0){
            close(fd);
            connections.erase(fd);
        }else{
            std::cout << "[INFO] Received " << bytes_read << " bytes from " << fd << std::endl;
        }
    }

    // forward request to destination server
    void _forward_upstream(){
        // TODO
    }
}

void Server::socket_init(){
    socket_p = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_p < 0){
        close(socket_p);
        throw std::runtime_error(std::string("server socket initialization failed: ") + std::string(strerror(errno)));
    }
    Proxy::_set_nonblocking(socket_p);
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

// epolling between proxy and client/requested server
void Server::server_run(const std::string& forbidden_sites_file, 
                        struct sockaddr_in &srv_addr){
    
    int epfd = epoll_create1(0);
    if(epfd == -1) {throw std::exception << "epoll_create1() error: " + std::string(strerror(errno));}

    Proxy::_listen();

    // proxy server epoll event handling
    struct epoll_event ev, events[MAX_CONNECTIONS];
    ev.events = EPOLLIN;
    ev.fd = socket_p;

    // TODO: SIGINT signal handling using sigset_t

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_p, &ev) == -1){ // add proxy server event to list of epoll structs 
        throw std::exception <<"failure adding proxy event to epoll: " + std::string(strerror(errno));
    }

    std::cout<<"[INFO] Running Proxy Server."<<std::endl;

    while(true){
        // TODO: handle connecting clients and responding servers
        int num_events = epoll_wait(epfd, events, MAX_CONNECTIONS, -1);
        for(int i = 0; i < num_events; i++){
            int fd = events[i].data.fd;

            if(fd == socket_p){ // client trying to connect
                while(true){
                    int client_fd = accept(socket_p, nullptr, nullptr);
                    if(client_fd < 0) {break;} // no clients trying to connect
                    Proxy::_set_nonblocking(client_fd);

                    // adding client epoll event to list of epoll structs
                    ev.events = EPOLLIN | EPOLLOUT;
                    ev.data.fd = client_fd;
                    if(epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1){ // add proxy server event to list of epoll structs 
                        throw std::exception <<"failure adding client event to epoll: " + std::string(strerror(errno));
                    }

                    std::cout<<"[INFO] New client connected: "<< client_fd<< endl;
                    connections[client_fd] = {client_fd, -1};
                }
            }
            // client sending request, forward to destination server
            else if((events[i].event & EPOLLIN) && connection.find(fd) != connection.end()){
 
            }
        }

    }
}
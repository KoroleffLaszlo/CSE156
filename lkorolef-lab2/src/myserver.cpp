#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/server.h"

struct Server server;
int main(int argc, char* argv[]){
    if(argc != 2){
        std::cerr<<"Incorrect number of arguments"<<std::endl;
        return EXIT_FAILURE;
    }

    struct sockaddr_in srv_addr;
    int srv_port = std::stoi(argv[1]);

    try{
        server.socket_init();
        server.server_bind(srv_addr, srv_port);
        server.server_recv();
    }catch(const std::exception& e){
        std::cerr<<"Error: "<< e.what() <<std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
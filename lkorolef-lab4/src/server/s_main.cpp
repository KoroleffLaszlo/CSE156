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

#include "../../include/server/server.h"

Server server;

int main(int argc, char* argv[]){
    if(argc != 4){
        std::cerr<<"Invalid number of commands"<<std::endl;
        return EXIT_FAILURE;
    }

    struct sockaddr_in srv_addr;
    int srv_port = std::stoi(argv[1]);
    int droppc = std::stoi(argv[2]);
    if(droppc > 100 || droppc < 0){
        std::cerr<<"Invalid packet drop member"<<std::endl;
        return EXIT_FAILURE;
    }
    try{
        server.socket_init();
        server.server_bind(srv_addr, srv_port);
        server.server_recv(droppc);
    }catch(const std::exception &e){
        std::cerr<<"Error: "<< e.what() <<std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
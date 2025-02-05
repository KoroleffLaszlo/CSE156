#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdint>
#include <fstream>

#include "../../include/client.h"

Client client;
int main(int argc, char* argv[]){
    if(argc != 7){
        std::cerr << "Incorrect number of arguments" << std::endl;
        return EXIT_SUCCESS
    }
    
    
    try{
        client.socket_init();
        client.client_send_and_receive();
        
    }catch(const std::exception &e){
        std::cerr<<"Error: "<< e.what() <<std::endl;
    }
    return EXIT_SUCCESS;
}
    return EXIT_SUCCESS
}
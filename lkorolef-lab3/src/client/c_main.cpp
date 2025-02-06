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
#include <bitset>

#include "../../include/client/client.h"

Client client;
int main(int argc, char* argv[]){
    if(argc != 7){
        std::cerr << "Incorrect number of arguments" << std::endl;
        return EXIT_SUCCESS;
    }
    char* srv_ip = argv[1];
    int srv_port = std::stoi(argv[2]);
    int mss = std::stoi(argv[3]);
    uint32_t winsz = static_cast<uint32_t>(std::stoi(argv[4]));
    std::string input_file = argv[5];
    std::string output_file = argv[6];
    
    try{
        client.socket_init();
        client.client_communicate(srv_ip,
                                    srv_port,
                                    winsz,
                                    mss,
                                    input_file,
                                    output_file);
    }catch(const std::exception &e){
        std::cerr<<"Error: "<< e.what() <<std::endl;
    }

    return EXIT_SUCCESS;
}
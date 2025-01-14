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
#include "../include/parse.h"

#define INPUT_MIN 3

Client client;
struct sockaddr_in client_addr;



std::string cmdtoStr(const int argc, const char* argv[]){
    std::string cmdStr;
    if (argc < INPUT_MIN) {
        throw std::runtime_error(std::string("Error: too few arguments provided"));
    }
    for (int i = 0; i <argc; ++i){
        cmdStr += argv[i];
        if (i < argc - 1) cmdStr += " "; 
    }
    return cmdStr;

}

int main(int argc, char* argv[]){
    std::string cmd
    std::vector<std::string> request_info;

    //parse cmd line and extract regex necessities 
    try{
        cmd = cmdtoStr(argc, argv);
        Parse::stringParse(cmd, request_info);
    }catch{}
    std::string website = request_info[0];
    std::string ip_address = request_info[1];
    std::string filepath = request_info[2];
    std::string port = request_info[3];
    std::string flag = request_info[4];

    try{

    }catch{}

    //init socket
    //establish connection to server
    //send request
    //recieve response
    //make money...

    
    return 0;
}
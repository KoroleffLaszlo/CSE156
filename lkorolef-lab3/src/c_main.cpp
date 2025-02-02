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

#include "../include/client.h"

int main(int argc, char* argv[]){
    if(argc != 7){
        std::cerr << "Incorrect number of arguments" << std::endl;
        return EXIT_SUCCESS
    }
    
    //TODO?: send metadata first (i.e. send out-file-path)
    return EXIT_SUCCESS
}
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

int main(int argc, char* argv[]){
    if (argc < INPUT_MIN) {
        std::cerr << "Error: Not enough arguments. At least 2 arguments are required.\n";
        return 1;
    }

    //init socket
    client.socket_init();

    //process cmd line
    std::string combinedArgs;

    for (int i = 0; i < argc; ++i) {
        combinedArgs += argv[i];  // Append the current argument
        if (i < argc - 1) {
            combinedArgs += " ";  // Add a space between arguments (optional)
        }
    }

    // Print the resulting combined string
    std::cout << "Combined string: " << combinedArgs << std::endl;
    
    //establish connection


    //send request
    //process request

    
    return 0;
}
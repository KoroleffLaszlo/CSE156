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

std::string cmdtoStr(int argc, char* argv[]) {
    if (argc < INPUT_MIN) {
        throw std::runtime_error("Error: too few arguments provided");
    }
    std::string cmdStr;
    for (int i = 0; i < argc; ++i) {
        cmdStr += argv[i];
        if (i < argc - 1) cmdStr += " ";
    }
    return cmdStr;
}

int main(int argc, char* argv[]) {
    std::string cmd;
    std::vector<std::string> request_info; // Initialize correctly
    std::string request_str;
    std::vector<char> buffer; // Initialize buffer
    Client client;
    struct sockaddr_in client_addr;

    try {
        cmd = cmdtoStr(argc, argv);
        Parse::stringParse(cmd, request_info); 

        //Parse::regex_debug(cmd);

        std::string host_url = request_info[0];
        std::string ip_address = request_info[1];
        std::string filepath = request_info[2];
        uint16_t port = static_cast<uint16_t>(std::stoi(request_info[3])); //str -> uint16_t for port
        std::string flag = request_info[4];

        client.socket_init();
        request_str = client.processReq(host_url, filepath, flag);
        client.connectToServer(client_addr, ip_address, port);
        client.sendToServer(request_str);
        client.recieveData(buffer, flag == "-h"); // Check if HEAD request
    } catch (const std::exception& e) {
        std::cerr << "Error in main(): " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
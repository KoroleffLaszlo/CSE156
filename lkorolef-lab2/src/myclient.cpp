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

#include "../include/file_t.h"
#include "../include/client.h"

#define MTU_MAX 31970

struct File_t file_handler;

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Incorrect number of arguments" <<std::endl;
        return EXIT_FAILURE;
    }

    char* srv_ip = argv[1];
    int srv_port = std::stoi(argv[2]);
    int mtu = std::stoi(argv[3]) - 2;
    std::string input_file = argv[4];
    std::string output_file = argv[5];

    if ((mtu <= 0) | (mtu > MTU_MAX)) {
        std::cerr<< "Incorrect MTU value" <<std::endl;
        return EXIT_FAILURE;
    }

    try {
        file_handler.fileRead(input_file, output_file, srv_ip, mtu, srv_port);
    } catch (const std::exception& e) {
        std::cerr<<"Error: "<< e.what() <<std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
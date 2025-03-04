#include "../../include/server/server.h"

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

#define MAX_CLIENTS 50

// namespace Debug {
//     // TODO: add debugging print for forbidden sites using on unordered_set
// }

namespace Helper {
    std::tuple<std::string, std::string, std::string> command_line_parse(int argc, char* argv) {
        std::string listen_port, forbidden_sites_file, log_file;
        int opt;

        while((opt = getopt(argc, argv, "p:a:l:")) != -1){
            switch(opt){
                case 'p':
                    listen_port = optarg;
                    break;
                case 'a':
                    forbidden_sites_file = optarg;
                    break;
                case 'l':
                    log_file = optarg;
                    break;
                default:
                    throw std::runtime_error << "expected: -p <port> -a <forbidden_sites_path> -l <access_log_path>\n";
            }
        }

        return {listen_port, forbidden_sites_file, log_file};
    }
}

Server server_handler;

int main(int argc, char* argv[]){

    struct sockaddr_in srv_addr;
    
    try{
        std::tuple<std::string, std::string, std::string> args = Helper::command_line_parse(argc, argv);

        int listen_port = std::stoi(std::get<0>(args)); // listening port: str -> int
        server_handler.socket_init();
        server_handler.server_bind(srv_addr, listen_port);

        // TODO: epolling for SIGINT to rescan forbidden files and monitor multiple clients
        

    }catch(const std::exception &e){
        std::cerr<<"Error - "<< e.what() <<std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
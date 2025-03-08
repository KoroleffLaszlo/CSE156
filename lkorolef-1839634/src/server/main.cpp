#include <unistd.h>
#include "../../include/server/server.h"
#include "../../include/common/thread.h"
#include "../../include/common/file_wrap.h"

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
#include <csignal>  
#include <fcntl.h>   

#define MAX_CLIENTS 50

Server server_handler;
_Thread thread_handler;

namespace Helper {
    std::tuple<std::string, std::string, std::string> command_line_parse(int argc, char* argv[]) {
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
                    throw std::runtime_error("expected: -p <port> -a <forbidden_sites_path> -l <access_log_path>");
            }
        }

        return {listen_port, forbidden_sites_file, log_file};
    }

    void handle_signal(int signum){
        std::cout<<"[INFO] Ctrl+C received -- updating forbidden sites"<<std::endl;
        server_handler.signal_flag.store(true, std::memory_order_relaxed);
    }
}

int main(int argc, char* argv[]){

    struct sockaddr_in srv_addr;
    std::signal(SIGINT, Helper::handle_signal);

    try{
        std::tuple<std::string, std::string, std::string> args = Helper::command_line_parse(argc, argv);
        server_handler.fsites =  File::file_read_stream(std::get<1>(args)); // assigns forbidden sites to global set for threads to access

        int listen_port = std::stoi(std::get<0>(args)); // listening port: str -> int
        server_handler.socket_init();
        server_handler.server_bind(srv_addr, listen_port);
        server_handler._listen(MAX_CLIENTS);
        std::cout<<"[INFO] Server running..."<<std::endl;
        
        while(true){
            if(server_handler.signal_flag.load(std::memory_order_relaxed)){
                server_handler.fsites = File::file_read_stream(std::get<1>(args)); // reload forbidden sites
                server_handler.signal_flag.store(false, std::memory_order_relaxed);
            }
            if(server_handler.client_count.load() >= MAX_CLIENTS){
                std::cout<<"[ERROR] Maximum client connections reached"<<std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1)); // sleep main process until threads are free
                continue;
            }

            std::unique_ptr<Server::Connection> _conn = server_handler.accept_client();
            
            if(!_conn) {continue;} // no clients attempting to connect -> go back and wait 

            std::thread client_thread = thread_handler.thread_create(&Server::server_run, 
                                                                    server_handler, 
                                                                    std::move(_conn));
            std::cout<<"[THREAD] "<<client_thread.get_id()<<std::endl;                                                   
            client_thread.detach(); // fire and forget
        }

    }catch(const std::exception &e){
        std::cerr<<"Error - "<< e.what() <<std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
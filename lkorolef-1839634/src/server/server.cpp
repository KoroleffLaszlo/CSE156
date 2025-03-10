#include "../../include/server/server.h"
#include "../../include/common/datagram.h"

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <map>
#include <utility>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/types.h>   
#include <sys/time.h>    
#include <unistd.h>      
#include <fcntl.h>
#include <bitset>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <shared_mutex>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>


#define MTU_MAX 32000

enum ErrorCheck {
    E_VALID = 10, // everything works as intended
    E_IP_RESBAD = 11, // ip resolved from getnameinfo() bad
    E_DOMAIN_RESBAD = 12, // domain resolved from getaddrinfo() bad
    E_FBID_REQ = 13, // forbidden request from client
    ERROR = 14 // catch all error handle
};

namespace Debug{
    // debugging checks for proper request creation
    void print_with_special_chars(const std::string& s){
        for(char c : s){
            if(c == '\r') {std::cout << "\\r";}  // show `\r`
            else if (c == '\n') {std::cout << "\\n";}  // show `\n`
            else {std::cout << c;}
        }
        std::cout << std::endl;
    }
}
namespace Resolve {
    // checks if destination server is given as ip address
    bool is_ip_address(const std::string& host) {
        struct sockaddr_in sa;
        return inet_pton(AF_INET, host.c_str(), &(sa.sin_addr)) == 1;
    }

    // reverse DNS lookup
    std::string resolve_ip_to_domain(const std::string &ip){
        struct sockaddr_in aip;
        char domain[NI_MAXHOST];
    
        memset(&aip, 0, sizeof(aip));
        aip.sin_family = AF_INET;
        inet_pton(AF_INET, ip.c_str(), &aip.sin_addr);
 
        if(getnameinfo((struct sockaddr*) &aip, sizeof(aip), domain, sizeof(domain), nullptr, 0, NI_NAMEREQD) != 0){
            std::cout<<"[ERROR] Ip address could not be resolved to existing domain"<<std::endl;
            return "";
        }
        return std::string(domain);
    }

    // DNS lookup
    std::string resolve_domain_to_ip(const std::string &domain){ 
        struct addrinfo filter{0}, *res;

        filter.ai_family = AF_INET;
        filter.ai_socktype = SOCK_STREAM;

        if(getaddrinfo(domain.c_str(), nullptr, &filter, &res) != 0){ // check if it exists
            std::cout<<"[ERROR] Domain could not be resolved to existing Ip address"<<std::endl;
            return "";
        }

        // grab mapping ip address
        char ip_str[INET_ADDRSTRLEN];
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, INET_ADDRSTRLEN);
    
        freeaddrinfo(res); 
        return std::string(ip_str);
    }
}

namespace Log {
    std::string timeStamp() {
        std::time_t now = std::time(nullptr);
        std::tm gmt = *std::gmtime(&now);  // Convert to UTC time
    
        std::ostringstream oss;
        oss << std::put_time(&gmt, "%Y-%m-%dT%H:%M:%SZ");  // RFC 3339 format
    
        return oss.str();
    }

    int log(const std::string &filePath, const std::string &method, const std::string &code){
        return 0;
    }
}

Server::Server() : socket_p(-1){
    openssl_init();
};

Server::~Server(){
    if(socket_p >= 0) {close(socket_p);}
    if(ssl_ctx) {cleanup_openssl();}
}

void Server::socket_init(){
    socket_p = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_p < 0){
        close(socket_p);
        throw std::runtime_error(std::string("server socket initialization failed: ") 
                + std::string(strerror(errno)));
    }
}

void Server::openssl_init(){
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    ssl_ctx = SSL_CTX_new(TLS_client_method());  // Create a client SSL context

    if(!ssl_ctx){
        std::cerr << "[ERROR] Failed to create OpenSSL context" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void Server::cleanup_openssl(){
    SSL_CTX_free(ssl_ctx);
    EVP_cleanup();
}

void Server::server_bind(struct sockaddr_in &srv_addr, const int& port){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    if((setsockopt(socket_p, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) < 0){
        close(socket_p);
        throw std::runtime_error(std::string("server SO_REAUSEADDR flag failed: ") + std::string(strerror(errno)));
    }

    if((bind(socket_p, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0)){
        close(socket_p);
        throw std::runtime_error(std::string("server bind failed: ") + std::string(strerror(errno)));
    }
}

void Server::_listen(int maxSize){
    if(listen(socket_p, maxSize) < 0){
        close(socket_p);
        throw std::runtime_error(std::string("server listen failed: ") + std::string(strerror(errno)));
    }
}

std::unique_ptr<Server::Connection> Server::accept_client(){
    struct sockaddr_in client;
    socklen_t client_size = sizeof(client);
    int fd = accept(socket_p, (struct sockaddr*)&client, &client_size);
    if(fd < 0) {
        if(errno == EINTR){ // signal interupt handling (blocking handling)
            std::cout<<"[ERROR] Signal interupt detected"<<std::endl;
        }else{
            std::cerr<<"[FATAL] Failed to connect to client: "<< strerror(errno) << std::endl;
        }
        return nullptr;
    }

     // thread safe conversion from bytes to char
     char ip_str[INET_ADDRSTRLEN];
     inet_ntop(AF_INET, &client.sin_addr, ip_str, INET_ADDRSTRLEN);
 
     std::cout<<"[INFO] Client connection accepted ";
     std::cout<<"- using fd: "<<fd<< std::endl;

     return std::make_unique<Connection>(fd, std::string(ip_str));
}

bool Server::is_exist(const std::string &host){
    std::shared_lock lock(fsites_mutex);
    if(fsites.find(host) != fsites.end()){ // if found 
        std::cout<<"[ERROR] Client requests forbidden"<<std::endl;
        return true;
    }

    if(Resolve::is_ip_address(host)){ // is ip?
        std::string domain = Resolve::resolve_ip_to_domain(host);
        if(domain == ""){ // invalid ip -- no domain 
            errno = E_IP_RESBAD;
            return true;
        }
        if(fsites.find(domain) != fsites.end()){ // domain in forbidden sites list
            return true;
        }else{
            return false;
        }
    }

    std::string ip_addr = Resolve::resolve_domain_to_ip(host);
    if(ip_addr == ""){ // invalid domain -- no ip
        errno = E_DOMAIN_RESBAD;
        return true;
    }
    if(fsites.find(ip_addr) != fsites.end()){ 
        return true;
    }

    return false;
}

std::string Server::forward_https_request(const std::string& host, const std::string& request){
    BIO* bio = BIO_new_ssl_connect(ssl_ctx);
    SSL* ssl;

    BIO_get_ssl(bio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    BIO_set_conn_hostname(bio, (host + ":443").c_str());

    if(BIO_do_connect(bio) <= 0){
        BIO_free_all(bio);
        return "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
    }

    if(BIO_do_handshake(bio) <= 0){
        BIO_free_all(bio);
        return "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
    }

    BIO_write(bio, request.c_str(), request.length());
    BIO_flush(bio);
    //std::cout<<"[INFO] Server sending to destination"<<std::endl;

    char response[4096];
    std::string result;
    int bytes_read;
    bool headers_received = false;

    while((bytes_read = BIO_read(bio, response, sizeof(response))) > 0){
        result.append(response, bytes_read);
        //std::cout <<"[INFO] Received: "<<bytes_read<<" bytes"<< std::endl;

        if(!headers_received && result.find("\r\n\r\n") != std::string::npos){
            headers_received = true;
            //std::cout<<"[INFO] Headers received -- checking content length..."<<std::endl;
        }

        // check for full response if content-length given
        size_t content_length_pos = result.find("Content-Length:");
        if(content_length_pos != std::string::npos){
            size_t end_of_headers = result.find("\r\n\r\n") + 4;
            int content_length = std::stoi(result.substr(content_length_pos + 15));
            if(result.size() >= end_of_headers + content_length){
                //std::cout<<"[INFO] Full response received -- stopping read"<<std::endl;
                break;
            }
        }
    }

    std::cout<<"[INFO] Finished receiving"<<std::endl;
    BIO_free_all(bio);
    return result.empty() ? "HTTP/1.1 504 Gateway Timeout\r\nContent-Length: 0\r\n\r\n" : result;
}

void Server::server_run(std::unique_ptr<Connection> t){

    client_count++;
    std::string _recv;

    char buffer[MTU_MAX];
    int client_socket = t->client_fd;

    // handling possible chunked client request
    while(_recv.find("\r\n\r\n") == std::string::npos){ // handles chunking
        int bytes_recv = recv(client_socket, buffer, sizeof(buffer), 0);
        _recv.append(buffer, bytes_recv);
    }

    std::pair<std::string, std::string> p = Dgram::get_method_and_host(_recv); // method -> "GET/HEAD" and host entry
    t->method = p.first;
    std::string host = p.second;
    t->request = Dgram::get_status_code(_recv); // grabbing status code for future logging 
    std::string req_to_server = Dgram::convert_to_relative_request(_recv); // formatting request to send to destination
    //std::cout<<"[REQ] Request body to server: \n";
    //Debug::print_with_special_chars(req);
    std::cout<<host<<std::endl;
    std::cout<<t->request<<std::endl;

    // TODO: proper logging with either errors or if in fsites
    if(is_exist(host)){ 
        if(errno == E_IP_RESBAD || errno == E_DOMAIN_RESBAD){ // bad gateway (host doesn't exist)
            std::cout<<"stopping"<<std::endl;
        }else{ // forbidden (in fsites)
            std::cout<<"stopping"<<std::endl;
        }
        client_count--;
        return;
    }
    // TODO: handle SSL with function and also additional logging
    std::string res = forward_https_request(host, req_to_server);// handle response back from server
    // std::cout<<"[INFO] Response from server: \n";
    // std::cout<<res<<std::endl;
    // std::cout<<std::endl;
    client_count--;
    return;
}
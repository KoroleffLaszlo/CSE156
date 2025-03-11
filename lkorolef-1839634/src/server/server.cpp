#include "../../include/server/server.h"
#include "../../include/common/datagram.h"
#include "../../include/common/file_wrap.h"

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
#include <tuple>   
#include <fcntl.h>
#include <bitset>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <shared_mutex>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <unistd.h>

#define MTU_MAX 32000

enum ErrorCheck {
    E_VALID = 10, // everything works as intended
    E_IP_RESBAD = 11, // ip resolved from getnameinfo() bad
    E_DOMAIN_RESBAD = 12, // domain resolved from getaddrinfo() bad
    E_FBID_REQ = 13, // forbidden request from client
    E_DISCONNECT = 14, // connection failed in communication
    E_ERROR = 15 // catch all error handle
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

namespace Log_handle {
    std::string timeStamp() {
        std::time_t now = std::time(nullptr);
        std::tm gmt = *std::gmtime(&now);  // Convert to UTC time
    
        std::ostringstream oss;
        oss << std::put_time(&gmt, "%Y-%m-%dT%H:%M:%SZ");  // RFC 3339 format
    
        return oss.str();
    }

    void _log(const std::unique_ptr<Server::Connection>& t){
        std::string message = t->time_stamp + " " + t->client_ip + " " +
                            t->request + " " + t->code + " " + t->content_length;
        File::file_write_stream(message);
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

// checks for cert file existance
bool Server::setup_ssl_certificates(SSL_CTX* ssl_ctx){
    const char* cert_file = X509_get_default_cert_file();
    const char* cert_dir = X509_get_default_cert_dir();
    if (!ssl_ctx) {
        std::cout << "[ERROR] SSL_CTX is NULL - Failed to initialize OpenSSL context" << std::endl;
        return false;
    }

    if(SSL_CTX_load_verify_locations(ssl_ctx, cert_file, cert_dir)) {return true;}

    // fallback method checks abs paths
    const char* ca_paths[] = {
        "/etc/ssl/certs/ca-certificates.crt",  // Debian, Ubuntu, Arch
        "/etc/pki/tls/certs/ca-bundle.crt",    // RHEL, CentOS, Fedora
        "/etc/ssl/cert.pem"                    // macOS
    };
    for(const char* path : ca_paths){
        if(access(path, F_OK) == 0){  // Check if the file exists
            if(SSL_CTX_load_verify_locations(ssl_ctx, path, NULL)){
                std::cout<<"[DEBUG] fallback SSL_CTX load location success"<<std::endl;
                return true;
            }
        }
    }
    return false;
}

void Server::openssl_init(){
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    ssl_ctx = SSL_CTX_new(TLS_client_method());  // create a client SSL context

    if(!ssl_ctx){
        throw std::runtime_error(std::string("failed to create OpenSSL context: ")
                + std::string(strerror(errno)));
    }

    if (!setup_ssl_certificates(ssl_ctx)) {
        std::cerr << "[ERROR] Failed to load CA certificates. HTTPS connections may not work..." << std::endl;
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
    std::cout << "\nIN ACCEPT" << std::endl;
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


    return std::make_unique<Connection>(fd, std::string(ip_str), std::ref(fsites));
}


bool Server::is_exist(const std::string &host){
    std::unordered_set<std::string> _fsites;
    { // smaller scope, copying updated fsites to use an instance of current snapshot
        std::shared_lock<std::shared_mutex> read_lock(fsites_mutex);
        // std::cout << "[DEBUG] Address of read_lock: " << &fsites_mutex << std::endl;
        // std::cout<<"ACQUIRED READ LOCK"<<std::endl;
        // sleep(8);
        // std::cout<< "\n\n\n" << "---------------------" << "\n\n\n" <<std::endl;
        // std::cout << "[DEBUG] Thread's forbidden site content:\n[";
        // for (const auto &i : *(fsites)){
        //     std::cout << i << std::endl;
        // }
        // std::cout << "]" << std::endl;
        // std::cout<< "\n\n\n" << "---------------------" << "\n\n\n" <<std::endl;

        _fsites = *fsites;
        read_lock.unlock();
        // sleep(8); //debug 
    }

    if(Resolve::is_ip_address(host)){ // is ip?
        std::string domain = Resolve::resolve_ip_to_domain(host);
        if(domain == ""){ // invalid ip -- no domain 
            errno = E_IP_RESBAD;
            return true;
        }
        if(_fsites.find(domain) != _fsites.end()) {return true;}
        else {return false;}
    }

    std::string ip_addr = Resolve::resolve_domain_to_ip(host);
    if(ip_addr == ""){ // invalid domain -- no ip
        errno = E_DOMAIN_RESBAD;
        return true;
    }
    if(_fsites.find(ip_addr) != _fsites.end()) {return true;}
    return false;
}

// creates connection between the proxy and the destination server
int Server::create_tcp_connection(const std::string &host){
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;

    std::string port;
    if((port = Dgram::extract_port(host)) == "") {port = "443";}
    // TODO: handle condition where client request specifies ssl port
    if(getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0){
        std::cerr<<"[ERROR] Failed to resolve host: "<<host<<std::endl;
        return -1;
    }
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sockfd < 0){
        std::cerr<<"[ERROR] Failed to create socket"<<std::endl;
        freeaddrinfo(res);
        return -1;
    }
    if(connect(sockfd, res->ai_addr, res->ai_addrlen) < 0){
        std::cerr<<"[ERROR] Failed to connect to "<<host<<std::endl;
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return sockfd;
}

std::string Server::forward_https_request(const std::string &host, const std::string &request, bool ignore_cert_errors) {
    // create tcp connection to server destination

    int sockfd = create_tcp_connection(host);
    if(sockfd < 0){
        return "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
    }

    // set up SSL pointer object to set up connection
    SSL *ssl = SSL_new(ssl_ctx);
    if(!ssl){
        std::cerr << "[ERROR] Failed to create SSL structure" << std::endl;
        close(sockfd);
        return "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
    }

    SSL_set_fd(ssl, sockfd);

    // if flag up ignore ssl ca cert messages
    if(ignore_cert_errors){
        SSL_set_verify(ssl, SSL_VERIFY_NONE, nullptr);
    }

    // SSL handshake
    if(SSL_connect(ssl) <= 0){
        std::cerr << "[ERROR] SSL handshake failed with " << host << std::endl;
        SSL_free(ssl);
        close(sockfd);
        return "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
    }

    if(SSL_write(ssl, request.c_str(), request.length()) <= 0){
        std::cerr<<"[ERROR] SSL_write failed"<<std::endl;
        SSL_free(ssl);
        close(sockfd);
        return "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
    }

    // reads response from server
    char buffer[8192];
    std::string response;
    int bytes_read;

    while((bytes_read = SSL_read(ssl, buffer, sizeof(buffer))) > 0){
        response.append(buffer, bytes_read);
    }

    if(bytes_read < 0){
        std::cerr<<"[ERROR] SSL_read failed"<<std::endl;
        response = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
    return response;
}

// handles sending responses back to the client
ssize_t Server::send_to_client(const std::unique_ptr<Connection>& t){
    int client_socket = t->client_fd;
    ssize_t bytes_sent = 0;
    ssize_t total_sent = 0;
    ssize_t response_size = t->response.size();
    std::string response = t->response;

    Log_handle::_log(t);

    while(total_sent < response_size){
        bytes_sent = send(client_socket, response.c_str() + total_sent, response_size - total_sent, 0);

        if(bytes_sent < 0){  // client disconnect/err
            std::cerr<<"[ERROR] Failed to send response to client: "<<strerror(errno)<<std::endl;
            return -1;
        }
        if(bytes_sent == 0){  // client closed connection
            std::cerr<<"[INFO] Client closed the connection before receiving full response."<<std::endl;
            return 0;
        }
        total_sent += bytes_sent;
    }

    return total_sent;
}

void Server::server_run(std::unique_ptr<Connection> t){

    client_count++;
    std::string _recv = "";
    int send_ret = 0;
    char buffer[MTU_MAX];
    int client_socket = t->client_fd;

    // handling possible chunked client request
    t->time_stamp = Log_handle::timeStamp();
    while(_recv.find("\r\n\r\n") == std::string::npos){ // handles chunking
        int bytes_recv = recv(client_socket, buffer, sizeof(buffer), 0);
        _recv.append(buffer, bytes_recv);
    }

    // method -> "GET/HEAD" | host entry | http version
    std::tuple<std::string, std::string, std::string> tp = Dgram::get_method_host_version(_recv);
    std::string method = std::get<0>(tp);
    std::string host = std::get<1>(tp);
    std::string version = std::get<2>(tp);

    t->request = Dgram::get_request(_recv); // grabbing response for future logging
    std::string req_to_server = Dgram::convert_to_relative_request(_recv, t->client_ip); // formatting request to send to destination

    if((method != "GET" && method != "HEAD") || (version != "HTTP/1.1" && version != "HTTPS/1.1")){
        t->response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n";
        t->code = "501";
        send_ret = send_to_client(std::move(t));
        if(send_ret <= 0) {std::cerr<<"[ERROR] Connection malformed to client"<<std::endl;}
        client_count--;
        return;
    }
   
    if(is_exist(host)){ 
        if(errno == E_IP_RESBAD || errno == E_DOMAIN_RESBAD){ // bad gateway (host doesn't exist)
            t->response = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
            t->code = "502";
            send_ret = send_to_client(std::move(t));
        }else{ // forbidden (in fsites)
            t->response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
            t->code = "403";
            send_ret = send_to_client(std::move(t));
        }
        if(send_ret <= 0) {std::cerr<<"[ERROR] Connection malformed to client"<<std::endl;}
        client_count--;
        return;
    }

    t->response = forward_https_request(host, req_to_server, i_cert_flag);// handle response back from server

    // rechecking with possibly updated forbidden sites
    if(is_exist(host)){ 
        if(errno == E_IP_RESBAD || errno == E_DOMAIN_RESBAD){ // bad gateway (host doesn't exist)
            t->response = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
            t->code = "502";
            send_ret = send_to_client(std::move(t));
        }else{ // forbidden (in fsites)
            t->response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
            t->code = "403";
            send_ret = send_to_client(std::move(t));
        }
        if(send_ret <= 0) {std::cerr<<"[ERROR] Connection malformed to client"<<std::endl;}
        client_count--;
        return;
    }

    // get status code and content length
    std::pair<std::string, std::string> p = Dgram::get_status_and_length(t->response);
    t->code = p.first;
    t->content_length = p.second;

    if((send_ret = send_to_client(std::move(t))) <= 0){
        std::cerr<<"[ERROR] Connection malformed to client"<<std::endl;
        client_count--;
        return;
    }

    // success 
    client_count--;
    return;
}
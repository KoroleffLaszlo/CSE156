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
#include "../include/log.h" //log handling to .dat file 

#define BUFFER_SIZE 2048

Client::Client() : socket_p(-1){};

Client::~Client(){
    if(socket_p >= 0) close(socket_p);
}

void Client::socket_init(){
    socket_p = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_p < 0){
        throw std::runtime_error(std::string("socket_init() failed: ") + std::string(strerror(errno)));
    }
}

void Client::ip_is_equal(const std::string& host_ip, const std::string& cmd_ip) {
    if (host_ip != cmd_ip) {
        throw std::runtime_error("ip_is_equal() failure: IP addresses do not match");
    }
}

std::string Client::host_ip_resolve(const std::string& host_url) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // resolve hostname to ip address
    int status = getaddrinfo(host_url.c_str(), nullptr, &hints, &res);
    if (status != 0) {
        throw std::runtime_error("getaddrinfo in host_ip_resolve() error: " + std::string(gai_strerror(status)));
    }
    // extract ip address
    char ip_str[NI_MAXHOST];
    if (getnameinfo(res->ai_addr, res->ai_addrlen, ip_str, sizeof(ip_str), nullptr, 0, NI_NUMERICHOST) != 0) {
        freeaddrinfo(res);
        throw std::runtime_error("getnameinfo in host_ip_resolve() error: " + std::string(strerror(errno)));
    }

    freeaddrinfo(res);
    return std::string(ip_str);
}

void Client::connectToServer(struct sockaddr_in &srv_addr, const std::string& ip, const uint16_t port){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
   // std::cout<<"SOCKET "<<socket_p<<std::endl;
    if(inet_pton(AF_INET, ip.c_str(), &srv_addr.sin_addr) <= 0){
        close(socket_p);
        throw std::runtime_error("Invalid address: " + std::string(strerror(errno)));
    }

    if(connect(socket_p, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0){
        close(socket_p);
        throw std::runtime_error("Connection failure: " + std::string(strerror(errno)));
    }
}

void Client::sendToServer(const std::string &request){
    if(send(socket_p, request.c_str(), request.length(), 0) < 0){
        close(socket_p);
        throw std::runtime_error("Request failure: " + std::string(strerror(errno)));
    }
}

void Client::recieveData(std::vector<char>& buffer, bool isHeadRequest) {
    char temp_buffer[BUFFER_SIZE];
    int err;
    std::string header;
    size_t header_end_pos = std::string::npos;
    size_t content_length = 0;
    size_t body_received = 0;
    bool header_parsed = false;

    try {
        while ((err = recv(socket_p, temp_buffer, BUFFER_SIZE, 0)) > 0) {
            buffer.insert(buffer.end(), temp_buffer, temp_buffer + err);

            if (!header_parsed) {
                std::string received_data(buffer.begin(), buffer.end());
                header_end_pos = received_data.find("\r\n\r\n");

                if (header_end_pos != std::string::npos) {
                    header = received_data.substr(0, header_end_pos + 4);
                    header_parsed = true;

                    if (isHeadRequest) {
                        std::cout << header << std::endl;
                        return;
                    }

                    size_t content_length_pos = header.find("Content-Length: ");
                    if (content_length_pos != std::string::npos) {
                        content_length_pos += 16; // Length of "Content-Length: "
                        size_t end_of_line = header.find("\r\n", content_length_pos);
                        content_length = std::stoul(header.substr(content_length_pos, end_of_line - content_length_pos));
                        
                    } else {
                        close(socket_p);
                        return;
                    }
                    
                    buffer.erase(buffer.begin(), buffer.begin() + header_end_pos + 4);
                    
                    std::string buffer_string(buffer.begin(), buffer.end());
                    body_received+= buffer_string.length();
                    Log::handle_response(buffer_string);
                }

            } else {
                body_received+= err;
            }

            // log web page to .dat file
            if (header_parsed && body_received == content_length){
                std::string response_body(buffer.begin(), buffer.end());
                Log::handle_response(response_body);
                close(socket_p);
                return;
            }
        }

        if (err < 0) {
            close(socket_p);
            throw std::runtime_error("Request failure: " + std::string(strerror(errno)));
        }

        if (header_parsed && body_received < content_length) {
            close(socket_p);
            throw std::runtime_error("Incomplete body response");
        }
    } catch (const std::exception& e){
        throw std::runtime_error("Error initializing log file: " + std::string(e.what()));
    }
}

std::string Client::processReq(const std::string& host_url, const std::string& filepath, const std::string& flag) const{
    std::string request;
    std::string filepath_p;

    if(filepath.empty()){ //to fix non given filepath
        filepath_p = "";
    }else filepath_p = filepath;

    if(flag == " -h"){
        request = "HEAD /" +  filepath_p + " HTTP/1.1\r\nHost: " + host_url + "\r\n\r\n";
        
    }
    else request = "GET /" +  filepath_p + " HTTP/1.1\r\nHost: " + host_url + "\r\n\r\n";

    return request;
}
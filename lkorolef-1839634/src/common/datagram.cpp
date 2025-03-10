#include "../../include/common/datagram.h"

#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include <algorithm>

// returns host
std::pair<std::string, std::string> Dgram::get_method_and_host(const std::string& s){
    std::istringstream iss(s);
    std::string method, host;
    iss>>method;
    std::string line;
    while(std::getline(iss, line) && line != "\r\n\r\n"){
        if(line.find("Host: ") == 0){
            host = line.substr(6); // extract host name
            host.erase(std::remove(host.begin(), host.end(), '\r'), host.end());
            host.erase(std::remove(host.begin(), host.end(), '\n'), host.end());
            break;
        }
    }
    if(host.empty()) {return std::pair<std::string, std::string>("","");}
    return std::pair<std::string, std::string>(method, host);
}

std::string Dgram::get_status_code(const std::string& s){
    std::istringstream iss(s);
    std::string method, host, version;
    iss>>method>>host>>version;
    return (method + " " + host + " " + version);
}

// converts to appropriate request to be sent to server
std::string Dgram::convert_to_relative_request(const std::string& request){
    std::istringstream stream(request);
    std::string method, url, http_version;
    stream >> method >> url >> http_version;

    // extract relative URL
    size_t pos = url.find("/", url.find("://") + 3);
    std::string relative_url = (pos != std::string::npos) ? url.substr(pos) : "/";
    std::string modified_request = method + " " + relative_url + " " + http_version;

    // Preserve headers except for `Proxy-Connection`
    std::string line;
    while(std::getline(stream, line) && line != "\r\n\r\n"){
        if(line.find("Proxy-Connection") == std::string::npos){
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
            modified_request += line + "\r\n";
        }
    }
    //std::cout<<modified_request<<std::endl;
    return modified_request;
}


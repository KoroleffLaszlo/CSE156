#include <iostream>
#include <string>
#include <cerrno>
#include <string>
#include <stdexcept>
#include <cstring>
#include <regex>
#include <vector>

#include "../include/parse.h"

const std::string DEFAULT_PORT = "80"; //avoid runtime errors

std::regex Parse::
    pattern(R"((www\.[\w\.\-]+)\s+(\d{1,3}(?:\.\d{1,3}){3})(?::(\d+))?\/([\w\.\-]+)( -h)?)");

Parse::Parse(){};

// void Parse::stringParse(const std::string& cmd, std::string& ip, std::string& port, std::string& filepath){
    
//     std::smatch matches;

//     if(std::regex_search(cmd, matches, pattern)){
//         ip = matches[1].str();
//         filepath = matches[3].str();
//         if(matches[2].str().empty()){
//             port = DEFAULT_PORT;
//             std::cout<<"Port not specified using Default Port 80"<< std::endl;
//         }else port = matches[2].str();
//     }
//     return;
// }
void Parse::stringParse(const std::string& cmd, std::vector<std::string>& client_buffer){
    
    std::smatch matches;

    if(std::regex_search(cmd, matches, pattern)){
        info_buffer.push_back(matches[1].str()); //ip address
        info_buffer.push_back(matches[3].str()); //filepath

        if(matches[2].str().empty()){ //port number
            info_buffer.push_back(DEFAULT_PORT);
            std::cout<<"Port not specified using Default Port 80"<< std::endl;
        }else info_buffer.push_back(matches[2].str());

        if(matches[4].str().empty()){ //flag set
            info_buffer.push_back("");
        }else info_buffer.push_back(matches[4].str());
    }
    return;
}

void Parse::regex_debug(const std::string& cmd){
    std::smatch matches;
    std::string ip, filepath, port;

    if(std::regex_search(cmd, matches, pattern)){
        ip = matches[1].str();
        filepath = matches[3].str();
        if(matches[2].str().empty()){
            port = DEFAULT_PORT;
            std::cout<<"Port not specified using Default Port 80"<< std::endl;
        }else port = matches[2].str(); 
    }

    std::cout << "Matched Groups:" << std::endl;
    for (size_t i = 0; i < matches.size(); ++i) {
        std::cout << "Group " << i << ": " << matches[i].str() << std::endl;
    }
    return;
}


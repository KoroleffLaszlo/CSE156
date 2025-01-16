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

std::regex Parse::pattern(
    R"((?:http:\/\/|https:\/\/)?(?:www\.)?([\w\.-]+\.[a-z]{2,}(?:\.[a-z]{2,})?)(\s+(\d{1,3}(?:\.\d{1,3}){3})(?::(\d+))?(\/[\w\.\-/]*)?)?( -h)?)"
);

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
void Parse::stringParse(const std::string& cmd, std::vector<std::string>& request_info){
    
    std::smatch matches;

    if(std::regex_search(cmd, matches, pattern)){
        request_info.push_back(matches[1].str()); //domain url
        request_info.push_back(matches[3].str()); //ip address
        request_info.push_back(matches[5].str()); //filepath

        if(matches[4].str().empty()){ //port number
            request_info.push_back(DEFAULT_PORT);
            //std::cout<<"Port not specified using Default Port 80"<< std::endl; //debug
        }else request_info.push_back(matches[4].str());

        if(matches[6].str().empty()){ //flag set
            request_info.push_back("");
        }else request_info.push_back(matches[6].str());
    }
    return;
}

void Parse::regex_debug(const std::string& cmd){
    std::smatch matches;

    if (std::regex_search(cmd, matches, pattern)) {
        std::cout << "Matched Groups:" << std::endl;
        std::cout << "Total Groups: " << matches.size() << std::endl;
        for (size_t i = 0; i < matches.size(); ++i) {
            std::cout << "Group " << i << ": " << matches[i].str() << std::endl;
        }
    } else {
        std::cout << "No match found." << std::endl;
    }

    return;
}


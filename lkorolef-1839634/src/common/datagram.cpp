#include "../../include/common/datagram.h"

#include <iostream>
#include <string>
#include <sstream>
#include <utility>

// returns type of request (GET/HEAD) and website 
std::pair<std::string, std::string> Dgram::decode_request(const std::string& s){
    std::istringstream iss(s);
    std::string type, site;
    iss>>type>>site;

    return std::make_pair(type, site);
}
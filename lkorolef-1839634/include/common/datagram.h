#ifndef DGRAM
#define DGRAM

#include <iostream>
#include <string>
#include <utility>

class Dgram{
public:
    static std::pair<std::string, std::string> decode_request(const std::string&);
};
#endif
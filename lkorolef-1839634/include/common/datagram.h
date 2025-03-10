#ifndef DGRAM
#define DGRAM

#include <iostream>
#include <string>
#include <utility>

class Dgram{
public:
    static std::pair<std::string, std::string> get_method_and_host(const std::string&);
    static std::string get_status_code(const std::string&);
    static std::string convert_to_relative_request(const std::string&);
};
#endif
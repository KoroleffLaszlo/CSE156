#ifndef PARSE
#define PARSE

#include <vector>
#include <string>
#include <cstring>
#include <regex>

class Parse{
private:
    static std::regex pattern;
public:
    Parse();

    static void stringParse(const std::string&, std::string&, std::string&, std::string&);
    static void regex_debug(const std::string&);
};
#endif
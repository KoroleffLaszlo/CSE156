#ifndef LOG
#define LOG

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>

class Log{
private:
    static std::string filename;
public:
    Log();

    static void log_response_to_file(const std::string&);
    static void handle_response(const std::string& response);
};
#endif
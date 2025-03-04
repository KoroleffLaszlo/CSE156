#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <bitset>
#include <filesystem>
#include <errno.h>  
#include <unordered_set>

#include "../../include/common/file_wrap.h"

namespace Helper{
    // removes leading and tailing whitespaces
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        size_t end = str.find_last_not_of(" \t\r\n");
        return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
    }
}

// returns unordered_set containing forbidden domains (for faster look-up)
std::unordered_set<std::string> File::file_read_stream(const std::string& filePath){
    std::ifstream file(filePath);
    if(!file){
        throw std::runtime_error(std::string("Failed to read from file: ") + std::string(strerror(errno)));
    }
    std::unordered_set<std::string> entries;
    std::string line;
    while(std::getline(file, line)){
        size_t comment = line.find('#');
        if(comment != std::string::npos) {line = line.substr(0, comment);} // remove ending comment
        
        line = Helper::trim(line);
        if(!line.empty()) {entries.insert(line);}
    }
    return entries;
}


int File::file_write_stream(std::ofstream& file, const std::vector<uint8_t>& data) {
    if(!file.is_open()){
        throw std::runtime_error("Failed to write to file. Invalid file stream");
    }
    if(!file.write(reinterpret_cast<const char*>(data.data()), data.size())){
        throw std::runtime_error("Error writing to file.");
    }
    return 0; // success
}
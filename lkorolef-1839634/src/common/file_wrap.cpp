#include "../../include/common/file_wrap.h"

#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <errno.h>  
#include <unordered_set>

namespace Helper{
    // removes leading and tailing whitespaces
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        size_t end = str.find_last_not_of(" \t\r\n");
        return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
    }
}

// returns unordered_set containing forbidden domains (for faster look-up)
// TODO: make file if not exist
std::unordered_set<std::string> File::file_read_stream(const std::string& filePath){
    std::filesystem::path path(filePath);

    // Ensure parent directory exists before opening the file (return empty set)
    if(!std::filesystem::exists(path.parent_path()) && !path.parent_path().empty()){
        std::filesystem::create_directories(path.parent_path());
        return std::unordered_set<std::string>();
    }
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

int File::file_write_stream(const std::string &filePath, const std::string &data){
    std::filesystem::path path(filePath);

    // Ensure parent directory exists before opening the file
    if(!std::filesystem::exists(path.parent_path()) && !path.parent_path().empty()){
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(filePath);
    if(!file){
        std::cerr<<"[ERROR] Cannot open file: "<<filePath << std::endl;
        return -1;
    }
    file<<data;
    if(!file){  // check if write failed
        std::cerr<<"[ERROR] Failed to write to file: "<<filePath<<std::endl;
        return -1;
    }
    file.close();
    return 0;
}
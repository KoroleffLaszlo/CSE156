#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cstdlib>
#include <netdb.h>
#include <unistd.h>
#include <cstdint>
#include <fstream>
#include <bitset>
#include <filesystem>
#include <map>

#include "../../include/common/file_t.h"

// returns vector of data read and read position
std::pair<std::vector<uint8_t>, int> File::file_read_stream(std::ifstream &file, int mss, int read_pos){
    std::ifstream file(filePath, std::ios::binary);
    if(!file){
        throw std::runtime_error(std::string("Failed to read from file: ") + std::string(strerror(errno)));
    }
    file.seekg(read_pos, std::ios::beg);
    std::vector<uint8_t> buffer(mss);
    file.read(reinterpret_cast<char*>(buffer.data()), mss);
    std::cout<<"IN READ"<<std::endl;
    std::cout<<"[";
    for(size_t i = 0; i < buffer.size(); i++){
        std::cout<<(char)buffer[i];
    }
    std::cout<<"]"<<std::endl;

    size_t bytesRead = file.gcount();
    buffer.resize(bytesRead);
    file.close();
    return {buffer, static_cast<int>(bytesRead)};
}

int File::file_write_stream(const std::string& filePath, const std::map<uint32_t, std::vector<uint8_t>>& buffer, int write_pos){
    std::filesystem::path path(filePath);
    std::fstream file;
    // Ensure parent directories exist
    if(!std::filesystem::exists(path.parent_path()) && !path.parent_path().empty()){
        std::filesystem::create_directories(path.parent_path());
    }
    if(write_pos == 0){
        // If writing from the start, truncate the file (clear its contents)
        file.open(filePath, std::ios::binary | std::ios::trunc | std::ios::out);
    }else{
        // Otherwise, open in read/write mode without truncating
        file.open(filePath, std::ios::binary | std::ios::in | std::ios::out);
    }
    // If file still doesn't exist, create it
    if(!file){
        file.open(filePath, std::ios::binary | std::ios::out);
        if (!file) {
            throw std::runtime_error("Failed to write to file: " + std::string(strerror(errno)));
        }
    }
    file.seekp(write_pos, std::ios::beg);
    for (const auto& [key, data] : buffer) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        write_pos += data.size();
    }
    file.close();
    return write_pos;
}



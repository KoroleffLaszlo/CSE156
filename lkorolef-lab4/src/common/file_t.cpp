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
#include <optional>
#include <memory>
#include <fcntl.h>
#include <errno.h>  


#include "../../include/common/file_t.h"

#define LOCK_FLAG 5

namespace{ // specific functions only used by file_t
    bool is_file_lock(const std::string& filePath) {
        int file = open(filePath.c_str(), O_WRONLY);  // try opening 
        if (file == -1) {
            return (errno == EACCES || errno == EAGAIN);  // if file already being written to by other client
        }
    
        close(file);
        return false;
    }
}
// std::vector<std::string> File::read_config(const std::string& config_file, int lines_read){
//     std::vector<uint8_t> server_info;
//     std::ifstream file(config_file);
//     std::string line;

//     return server_info;
// }

std::unique_ptr<std::ofstream> File::open_file_stream(const std::string& filePath){
    std::filesystem::path path(filePath);
    // Ensure parent directories exist
    if(!std::filesystem::exists(path.parent_path()) && !path.parent_path().empty()){
        std::error_code err;
        if(!std::filesystem::create_directories(path.parent_path(), err)){
            throw std::runtime_error("Failed to create directory - " + filePath + " -- Error: " + err.message());
        }
    }
    // check if writing steam is used by other process (multiple clients)
    if(::is_file_lock(filePath)){
        return nullptr;
    }
    // Open file
    auto file = std::make_unique<std::ofstream>(filePath, std::ios::binary | std::ios::trunc);
    if(!file->is_open()){
        throw std::runtime_error("Failed to open file: " + filePath + " | Error: " + std::strerror(errno));
    }
    return file;
}

// returns vector of data read and read position
std::pair<std::vector<uint8_t>, int> File::file_read_stream(std::string filePath, 
                                                            int mss, 
                                                            int read_pos){
    std::ifstream file(filePath, std::ios::binary);
    if(!file){
        throw std::runtime_error(std::string("Failed to read from file: ") + std::string(strerror(errno)));
    }
    file.seekg(read_pos, std::ios::beg);
    std::vector<uint8_t> buffer(mss);
    file.read(reinterpret_cast<char*>(buffer.data()), mss);

    // std::cout<<"[";
    // for(size_t i = 0; i < buffer.size(); i++){
    //     std::cout<<(char)buffer[i];
    // }
    // std::cout<<"]"<<std::endl;

    size_t bytesRead = file.gcount();
    buffer.resize(bytesRead);
    file.close();
    return {buffer, static_cast<int>(bytesRead)};
}

int File::file_write_stream(std::ofstream& file, 
                            const std::map<uint32_t, std::vector<uint8_t>>& buffer){
    if(!file.is_open()){
        throw std::runtime_error("Failed to write to file. Invalid file stream");
    }

    // Write the entire buffer to the file
    for(const auto& [key, data] : buffer){
        if (!file.write(reinterpret_cast<const char*>(data.data()), data.size())) {
            throw std::runtime_error("Error writing to file.");
        }
    }
    return 0; // success
}
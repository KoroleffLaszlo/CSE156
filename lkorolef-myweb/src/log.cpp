#include <iostream>
#include <fstream>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>  
#include <filesystem>

#include "../include/log.h"

std::string Log::filename = "output.dat";

Log::Log() {
    try {
        std::filesystem::create_directories("../bin");
        std::ofstream file(Log::filename, std::ios::out);
        
        if (file) {
            file.close();
        } else {
            std::cerr << "Warning: Failed to initialize log file: " << Log::filename << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error initializing log file: " << e.what() << std::endl;
    }
}

void Log::log_response_to_file(const std::string& response) {
    int fd = open(Log::filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        std::cerr << "Warning: Could not write to log file: " << Log::filename << std::endl;
        return;
    }
    write(fd, response.c_str(), response.size());
    write(fd, "\n", 1);

    close(fd);
}

void Log::handle_response(const std::string& response) {
    //std::cout << response << std::endl; //for debug
    log_response_to_file(response);
}
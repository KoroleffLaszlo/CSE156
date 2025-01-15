#include <iostream>
#include <fstream>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>  
#include <filesystem>

#include "../include/log.h"

std::string Log::filename = "bin/output.dat";

Log::Log() {
    try {
        // Ensure the ../bin directory exists
        std::filesystem::create_directories("../bin");

        // Create or overwrite the file in ../bin/output.dat
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
    // Open the file in ../bin/output.dat with write permissions
    int fd = open(Log::filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        std::cerr << "Warning: Could not write to log file: " << Log::filename << std::endl;
        return;
    }

    // Write the response to the file
    write(fd, response.c_str(), response.size());
    write(fd, "\n", 1);

    // Close the file descriptor
    close(fd);
}

void Log::handle_response(const std::string& response) {
    std::cout << response << std::endl;
    log_response_to_file(response);
}

void Log::handle_error(const std::string& error) {
    std::cerr << error << std::endl;
    log_response_to_file(error);
}

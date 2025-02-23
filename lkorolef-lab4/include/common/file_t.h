#ifndef FILE_T
#define FILE_T
#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cstdlib>
#include <unistd.h>
#include <cstdint>
#include <fstream>
#include <memory>

class File{
public:
    std::unique_ptr<std::ofstream> open_file_stream(const std::string&);
    std::pair<std::vector<uint8_t>, int> file_read_stream(std::string, int, int);
    int file_write_stream(std::ofstream&, const std::map<uint32_t, std::vector<uint8_t>>&);
};
#endif
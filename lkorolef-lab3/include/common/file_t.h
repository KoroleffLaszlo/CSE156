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

class File{
public:
    std::pair<std::vector<uint8_t>, int> file_read_stream(std::string, int, int);
    int file_write_stream(const std::string&, const std::map<uint32_t, std::vector<uint8_t>>&, int);
};
#endif
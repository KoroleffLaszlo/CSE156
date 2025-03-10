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
#include <unordered_set>

class File{
public:
    static std::unordered_set<std::string> file_read_stream(const std::string&);
    static int file_write_stream(const std::string&, const std::string&);
};
#endif
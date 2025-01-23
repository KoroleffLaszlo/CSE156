#ifndef FILE_T
#define FILE_T
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

class File_t{
public:
    void fileRead(const std::string&,
                const std::string&,
                const char*,
                const int&,
                const int&);
};
#endif
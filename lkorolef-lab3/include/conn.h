#ifndef CONNECT
#define CONNECT

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdint>
#include <fstream>

#include <sys/socket.h>
#include <netinet/in.h>

class Conn{
private:  
    struct ClientState{
        //std::ofstream outFile; //no need maybe? just send file path to future writefile function
        std::string filePath; 
        uint16_t winSize;
        //[seq_num] -> data 
        std::map<uint16_t, std::vector<uint8_t>> buffer;

        //constructor
        ClientState(const std::string& path = "", uint16_t size = 0)
            : outFilePath(path), winSize(size) {}
    };

    //[ip_addr, port_num] -> struct
    std::map<std::pair<std::string, uint16_t>, ClientState> clients;

public:
    Conn();
    ~Conn();
    
    void addClient(const std::string&, uint16_t, const std::string&, uint16_t);
    bool clientExists(const std::string&, uint16_t) const;
    const ClientState* getClientState(const std::string&, uint16_t) const;
    ClientState* getClientState(const std::string&, uint16_t);
    void removeClient(const std::string&, uint16_t);
};
#endif
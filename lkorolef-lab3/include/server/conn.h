#ifndef CONNECT
#define CONNECT

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <map>

#include <sys/socket.h>
#include <netinet/in.h>

class Conn{
public:
    Conn();
    ~Conn();

    struct ClientState{
        //std::ofstream outFile; //no need maybe? just send file path to future writefile function
        std::string filePath; 
        uint16_t winSize;
        int write_pos; //saved write position for each file
        //[seq_num] -> data 
        std::map<uint16_t, std::vector<uint8_t>> buffer;

        ClientState(const std::string& path = "", uint16_t size = 0, int pos = 0)
            : filePath(path), winSize(size), write_pos(pos){}
    };
    
    void addClient(const std::string&, uint16_t, const std::string&, uint16_t);
    bool clientExists(const std::string&, uint16_t) const;
    const ClientState& getClientState(const std::string&, uint16_t) const;
    ClientState& getClientState(const std::string&, uint16_t);
    void removeClient(const std::string&, uint16_t);

private:  
    //[ip_addr, port_num] -> struct
    std::map<std::pair<std::string, uint16_t>, ClientState> clients;
};
#endif
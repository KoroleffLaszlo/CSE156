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
        std::string filePath; //output filepath
        uint32_t winSize;
        uint32_t base_seq_num; // starting window seq_num position
        uint32_t expected_seq_num;
        //[seq_num] -> data 
        std::map<uint32_t, std::vector<uint8_t>> buffer;

        ClientState(const std::string& path = "", uint32_t init = 0, int pos = 0)
            : filePath(path), winSize(init), base_seq_num(init), expected_seq_num(init){}
    };

    void addClient(const std::string&, uint16_t, const std::string&, uint32_t);
    bool clientExists(const std::string&, uint16_t) const;
    const ClientState& getClientState(const std::string&, uint16_t) const;
    ClientState& getClientState(const std::string&, uint16_t);
    void removeClient(const std::string&, uint16_t);

private:  
    //[ip_addr, port_num] -> struct
    std::map<std::pair<std::string, uint32_t>, ClientState> clients;
};
#endif
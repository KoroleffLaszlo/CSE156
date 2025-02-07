#ifndef CLIENT
#define CLIENT

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdint>
#include <fstream>

#include <sys/socket.h>
#include <netinet/in.h>

class Client{
private:
    int socket_p;
public:
    Client();
    ~Client();

    struct dgram_t{
        uint32_t sequence_num;
        std::vector<uint8_t> data_body;
    };
    std::string getCurrentRFC3339Time();
    void printLogLine(const std::string&, const std::string&, uint32_t);
    void socket_init();
    void client_send_packet(struct sockaddr_in&, 
                                int,
                                uint32_t, 
                                std::vector<uint8_t>, 
                                const socklen_t&);
    void waitForAck(struct sockaddr_in&, socklen_t);
    void client_communicate(const char*,
                    const int&,
                    const uint32_t&,
                    const int&,
                    const std::string&,
                    const std::string&);
};
#endif
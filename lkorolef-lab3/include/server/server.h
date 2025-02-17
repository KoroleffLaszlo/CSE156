#ifndef SERVER
#define SERVER

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdint>
#include <fstream>

#include <sys/socket.h>
#include <netinet/in.h>

class Server{
private:
    int socket_p;
public:
    Server();
    ~Server();
    void logPacketEvent(const std::string&, uint32_t);
    bool should_drop();
    void socket_init();
    void server_bind(struct sockaddr_in&, const int&);
    void retransmission(struct sockaddr_in&, uint32_t, const socklen_t&);
    void server_send_ack(struct sockaddr_in&, 
                    uint32_t,
                    const socklen_t&,
                    int);
    void server_recv(const int&);
    
};
#endif
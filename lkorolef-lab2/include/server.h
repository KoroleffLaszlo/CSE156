#ifndef SERVER
#define SERVER

#include <vector>
#include <cstdint>
#include <string>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>

class Server{
private:
    int socket_p;
public:
    Server();
    ~Server();

    void socket_init();
    void server_bind();
    void server_recv();
    void server_send();

}
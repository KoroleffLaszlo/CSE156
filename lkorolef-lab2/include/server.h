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
    void server_bind(struct sockaddr_in&, const int);
    void server_send(struct sockaddr_in&, const std::vector<uint8_t>&, const socklen_t&);
    void server_recv();
};
#endif
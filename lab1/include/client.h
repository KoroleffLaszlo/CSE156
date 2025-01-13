#ifndef CLIENT
#define CLIENT

#include <vector>
#include <cstdint>
#include <string>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>

class Client{
private:
    int socket_p;
public:
    Client();
    ~Client();

    void socket_init();
    void connectToServer(struct sockaddr_in&, const std::string&, uint16_t);
    void sendToServer(const std::string&);
    void recieveData(std::vector<char>&);
};
#endif
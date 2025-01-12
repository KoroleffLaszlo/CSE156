#ifndef CLIENT
#define CLIENT

#include <vector>
#include <cstdint>
#include <string>

class Client{
public:
    static int socket_init();
    static void connectToServer(int, struct sockaddr_in&, std::string, uint16_t);
    static void sendToServer(int, std::string);
    static void recieveData(int, std::vector<char>&);
};
#endif
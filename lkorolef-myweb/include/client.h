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
    void ip_is_equal(const std::string&, const std::string&);
    std::string host_ip_resolve(const std::string&);
    void connectToServer(struct sockaddr_in&, const std::string& , const uint16_t);
    void sendToServer(const std::string&);
    void recieveData(std::vector<char>&, bool);
    std::string processReq(const std::string&, const std::string&, const std::string&) const;
};
#endif
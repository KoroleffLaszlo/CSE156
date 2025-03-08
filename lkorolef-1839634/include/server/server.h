#ifndef SERVER
#define SERVER

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <atomic>

#include <sys/socket.h>
#include <netinet/in.h>

class Server{
private:
    int socket_p;
public:
    Server();
    ~Server();

    struct Connection { // maintains cross communication tracking
        int client_fd;
        int d_server_fd; // destination server port
        std::string client_ip;
        std::string request; // client request -> "GET http://ucsc.edu/ HTTP/1.1"
        std::string response; // destination server response

        Connection(int fd, std::string ip)
            : client_fd(fd), d_server_fd(-1), client_ip(ip), request(""), response(""){}
    };

    std::atomic<int> client_count = 0;
    std::atomic<bool> signal_flag{false};
    std::unordered_set<std::string> fsites;
    std::shared_mutex fsites_mutex;

    void socket_init();
    void server_bind(struct sockaddr_in&, const int&);
    void _listen(int maxSize);
    std::unique_ptr<struct Connection> accept_client();
    void server_run(std::unique_ptr<Connection>);
};
#endif

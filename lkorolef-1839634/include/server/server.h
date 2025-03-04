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

    struct Connection { // maintains cross communication tracking
        int client_fd;
        int d_server_fd; // destination server port
        std::string request; // client request
        std::string response; // destination server response

        Connection()
            : client_fd(-1), d_server_fd(-1), request(""), response(""){}
    };

    // mapping client_fds to Connection struct 
    std::unordered_map<int, Connection> connections;

    void socket_init();
    void server_bind(struct sockaddr_in&, const int&);
    void server_listen(int maxSize);
    int get_socket_p() const; 
};
#endif

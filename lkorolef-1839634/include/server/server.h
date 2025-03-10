#ifndef SERVER
#define SERVER

#include <iostream>
#include <cstdint>
#include <string>
#include <memory>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netinet/in.h>

class Server{
private:
    int socket_p;
    SSL_CTX* ssl_ctx;  // shared context used for secure session 
        
public:
    Server();
    ~Server();

    struct Connection { // maintains cross communication tracking
        int client_fd;
        // int d_server_fd; // destination server port
        std::string client_ip;
        std::string method;
        std::string request; // client request -> "GET http://ucsc.edu/ HTTP/1.1"
        std::string response; // destination server response
        std::string content_length;
        std::string time_stamp;

        Connection(int fd, std::string ip)
            : client_fd(fd), 
            //d_server_fd(-1), 
            client_ip(ip), 
            method(""),
            request(""), 
            response(""),
            content_length(""),
            time_stamp(""){}
    };

    std::atomic<int> client_count = 0;
    std::atomic<bool> signal_flag{false};
    std::unordered_set<std::string> fsites;
    std::shared_mutex fsites_mutex;
    std::string logFile;

    void socket_init();
    void openssl_init();
    void cleanup_openssl();
    void server_bind(struct sockaddr_in&, const int&);
    void _listen(int maxSize);
    std::unique_ptr<struct Connection> accept_client();
    bool is_exist(const std::string&);
    std::string forward_https_request(const std::string&, const std::string&);
    void server_run(std::unique_ptr<Connection>);
};
#endif

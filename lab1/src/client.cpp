#include <iostream.h>
#include <stdlib.h>
#include <cstring.h>
#include <cerrno.h>

#include "client.h"

int socket(){
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if(soc < 0){
        perror("socket_init() Error");
        std::cerr << "Error message: " << strerror(cerr) << std::endl;
        return -1;
    }

    return soc;
}

int connect(int soc, struct sockaddr_in &srv_addr, const char* ip, uint_16 port){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port)
    if(inet_pton(AF_NET, ip, &srv_addr.sin_addr) < 0){
        perror("Invalid address");
        std::cerr << "Error message: " << strerror(cerr) << std::endl;
        close(soc)
        return -1;
    }

    if(connect(soc, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0){
        perror("Connection to server Error");
        std::cerr << "Error message: " << strerror(cerr) << std::endl;
        close(soc)
        return -1;
    }

    return 0;
}

int send(int soc, const char* request){
    if(send(soc, request, strlen(request), 0) < 0){
        perror("get_request() Error");
        std:cerr << "Error message: " << strerrror(cerr) << std::endl;
        close(soc)
        return -1
    }

    return 0;
}




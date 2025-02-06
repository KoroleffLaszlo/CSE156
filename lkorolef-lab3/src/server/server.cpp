#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <map>
#include <utility>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/types.h>   
#include <sys/time.h>    
#include <unistd.h>      
#include <fcntl.h>     
#include <bitset>

#include "../../include/server/server.h"
#include "../../include/server/conn.h"
#include "../../include/common/datagram.h"
#include "../../include/common/file_t.h"

#define MTU_MAX 32000
#define TIMEOUT 3
#define META_FLAG 0
#define FIN_FLAG 3 // client finished terminate connection

Conn conn;
Dgram dgram;
File file_handle;

#define MTU_MAX 32000
Server::Server() : socket_p(-1){};

Server::~Server(){
    if(socket_p >= 0) close(socket_p);
}

void Server::socket_init(){
    socket_p = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_p < 0){
        close(socket_p);
        throw std::runtime_error(std::string("server socket initialization failed: ") + std::string(strerror(errno)));
    }
}

void Server::server_bind(struct sockaddr_in &srv_addr, const int& port){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    if((bind(socket_p, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0)){
        close(socket_p);
        throw std::runtime_error(std::string("server bind failed: ") + std::string(strerror(errno)));
    }
}

void Server::server_send_ack(struct sockaddr_in &client_addr, uint32_t seq_num, const socklen_t &addr_len){
    std::vector<uint8_t> ack_packet = dgram.encode_ack_packet(seq_num);
    int bytes_sent = sendto(socket_p, ack_packet.data(), ack_packet.size(), 0, (struct sockaddr*)&client_addr, addr_len);
    if(bytes_sent < 0){
        close(socket_p);
        throw std::runtime_error(std::string("Server bytes sent failed: ") + std::string(strerror(errno)));
    }
    std::cout<<"sent ack packet to client"<<std::endl;
    return;
}

void Server::server_recv(const int& droppc){
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t addr_len = (socklen_t)sizeof(client_addr);

    while(1){
        std::vector<uint8_t> buffer(MTU_MAX);
        memset(&client_addr, 0, sizeof(client_addr));
        int bytes_read = recvfrom(socket_p, buffer.data(), buffer.size(), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (bytes_read < 0){
            close(socket_p);
            throw std::runtime_error(std::string("Server bytes received failed: ") + std::string(strerror(errno)));
        }
        buffer.resize(bytes_read);

        // simulate packet dropping
        int rand_num = rand() % 100;
        if(rand_num < droppc){ // TODO: Log dropped packets 
            continue;
        }

        std::string client_ip = inet_ntoa(client_addr.sin_addr);
        uint16_t client_port = ntohs(client_addr.sin_port);

        // creating Client profile (clientState struct) for new/existing client
        if(buffer[0] == META_FLAG){
            std::cout<<"META PACKET"<<std::endl;
            std::pair<uint32_t, std::string> meta_data = dgram.decode_meta_packet(buffer);
            conn.addClient(client_ip, client_port, meta_data.second, meta_data.first);
            server_send_ack(client_addr, 0, addr_len); // send ack for meta (sequence num is 0)

            Conn::ClientState& clientDebug = conn.getClientState(client_ip, client_port);
            std::cout<<"Client's output file: "<<clientDebug.filePath<<std::endl;
            std::cout<<"Client's window size: "<<static_cast<int>(clientDebug.winSize)<<std::endl;
            std::cout<<"---------------"<<std::endl;
            continue;
        }
        
        Conn::ClientState& clientState = conn.getClientState(client_ip, client_port);
        if(buffer[0] == FIN_FLAG){
            std::cout<<"TERMINATING"<<std::endl;
            uint32_t fin_seq_num = dgram.decode_fin_packet(buffer);
            server_send_ack(client_addr, fin_seq_num, addr_len);
            clientState.write_pos = file_handle.file_write_stream(clientState.filePath, clientState.buffer, clientState.write_pos);
            conn.removeClient(client_ip, client_port);
            continue;
        }

        //else data packet
        //data packets: [0]:flag; [1-4]:seq_num; [5-end]: data-body
        uint32_t seq_num = dgram.decode_bytes(std::vector<uint8_t>(buffer.begin() + 1, buffer.begin() + 5));
        
        // ACK lost in response to client, don't duplicate -> resend ACK
        if(clientState.buffer.find(seq_num) != clientState.buffer.end()){
            server_send_ack(client_addr, seq_num, addr_len);
        }else{
            // is seq_num from prev window
            if(seq_num < clientState.base_seq_num){
                server_send_ack(client_addr, seq_num, addr_len);
                continue; // accept new packets (back to top)
            }else{
                // else store into clientState map and send ACK
                clientState.buffer[seq_num] = std::vector<uint8_t>(buffer.begin() + 5, buffer.end());
                server_send_ack(client_addr, seq_num, addr_len);
            }
        }
        if(clientState.buffer.size() == clientState.winSize){
            std::cout<<"MOVING WINDOW"<<std::endl;
            clientState.write_pos = file_handle.file_write_stream(clientState.filePath, clientState.buffer, clientState.write_pos);
            clientState.base_seq_num = std::prev(clientState.buffer.end())->first + 1; // Get the last key
            clientState.buffer.clear();
            continue;
        }
    }
}

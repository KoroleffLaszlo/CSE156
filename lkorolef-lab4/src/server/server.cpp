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
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <filesystem>


#include "../../include/server/server.h"
#include "../../include/server/conn.h"
#include "../../include/common/datagram.h"
#include "../../include/common/file_t.h"

#define MTU_MAX 32000
#define TIMEOUT 3

#define META_FLAG 0
#define DATA_FLAG 1
#define FIN_FLAG 3 // client finished terminate connection
#define FILE_LOCK_FLAG 9 // file writing busy

Conn conn;
Dgram dgram;
File file_handle;

#define MTU_MAX 32000
Server::Server() : socket_p(-1){};

Server::~Server(){
    if(socket_p >= 0) close(socket_p);
}

std::map<std::string, int> active_files; //maps if file is being written to to original client 

double drop_rate;
int total_packets_processed = 0;
int dropped_count = 0;

bool should_drop(){
    total_packets_processed++;
    double current_drop_rate = (double)dropped_count / total_packets_processed;
    //std::cout << " -- " << current_drop_rate << "; " << dropped_count << "; "<< total_packets_processed<<std::endl;
    //std::cout<<"RATE : "<< drop_rate<<std::endl;
    if(current_drop_rate < drop_rate){
        //std::cout<<"DROPPED: "<<current_drop_rate<<" < "<<drop_rate<<std::endl;
        dropped_count++;
        return true;
    }
    return false;
}

namespace{

    uint16_t getLocalPort(int socket_fds) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        
        if (getsockname(socket_fds, (struct sockaddr*)&addr, &addr_len) == -1) {
            perror("getsockname failed");
            return 0;
        }
        
        return ntohs(addr.sin_port); // Convert from network byte order to host byte order
    }

    void logPacketEvent(const std::string& type, 
        int socket_fds,
        struct sockaddr_in &srv_addr, 
        uint32_t pkt_sn){

        uint16_t lport = ::getLocalPort(socket_fds);
        uint16_t rport = ntohs(srv_addr.sin_port);
        std::string rip = static_cast<std::string>(inet_ntoa(srv_addr.sin_addr));
        // get current timestamp in RFC 3339 format
        std::time_t now = std::time(nullptr);
        std::tm* utcTime = std::gmtime(&now);
        
        char buffer[30];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000Z", utcTime);
        std::string timestamp(buffer);
    
        std::string logEntry = timestamp + ", " 
        + std::to_string(lport) + ", " + rip + ", " 
        + std::to_string(rport) + ", " + type + ", " 
        + std::to_string(pkt_sn) + "\n";

        std::cout << logEntry;
    }

    // signal that the file is already being writen to
    bool write_busy(int socket_p, struct sockaddr_in &client_addr, uint32_t seq_num, const socklen_t &addr_len){
        std::vector<uint8_t> data_body = dgram.encode_bytes(seq_num);
        data_body.insert(data_body.begin(), FILE_LOCK_FLAG);
        int bytes_sent = sendto(socket_p, data_body.data(), data_body.size(), 0, (struct sockaddr*)&client_addr, addr_len);
        if(bytes_sent < 0){
            return false;
        }
        return true;
    }

    bool server_send_ack(int socket_p, struct sockaddr_in &client_addr, uint32_t seq_num, const socklen_t &addr_len, int droppc){
        if(should_drop()){
            ::logPacketEvent("DROP ACK", socket_p, client_addr, seq_num);
            return true;
        }
        std::vector<uint8_t> ack_packet = dgram.encode_ack_packet(seq_num);
        int bytes_sent = sendto(socket_p, ack_packet.data(), ack_packet.size(), 0, (struct sockaddr*)&client_addr, addr_len);
        if(bytes_sent < 0){
            return false;
        }
        ::logPacketEvent("ACK", socket_p, client_addr, seq_num);
        return true;
    }

    bool write_window(Conn::ClientState* clientState){
        if(!clientState->fileStream || !clientState->fileStream->is_open()){
            return false;
        }
        std::vector<uint8_t> writeBuffer;
        // iterate from base seq num
        while(clientState->buffer.find(clientState->base_seq_num) != clientState->buffer.end()){
            std::vector<uint8_t>& data = clientState->buffer[clientState->base_seq_num];
            writeBuffer.insert(writeBuffer.end(), data.begin(), data.end()); // append writeBuffer
            clientState->buffer.erase(clientState->base_seq_num); // slide window (remove base from buffer)
            clientState->base_seq_num++; // move to next seq num
        }
        // if theres data to write we write in one pass
        if(!writeBuffer.empty()){
            file_handle.file_write_stream(*clientState->fileStream, writeBuffer);
        }
        return true;
    }
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

void Server::server_recv(const int& droppc, const std::string& root_path){
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t addr_len = (socklen_t)sizeof(client_addr);
    drop_rate = droppc / 100.0;
    
    while(1){
        std::vector<uint8_t> buffer(MTU_MAX);
        memset(&client_addr, 0, sizeof(client_addr));
        int bytes_read = recvfrom(socket_p, buffer.data(), buffer.size(), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (bytes_read < 0){
            close(socket_p);
            throw std::runtime_error(std::string("Server bytes received failed: ") + std::string(strerror(errno)));
        }
        buffer.resize(bytes_read);

        std::string client_ip = inet_ntoa(client_addr.sin_addr);
        uint16_t client_port = ntohs(client_addr.sin_port);

        // creating Client profile (clientState struct) for new/existing client
        if(buffer[0] == META_FLAG){

            if(should_drop()){
                ::logPacketEvent("DROP DATA", socket_p, client_addr, static_cast<uint32_t>(0));
                continue;
            }
            std::pair<uint32_t, std::string> meta_data = dgram.decode_meta_packet(buffer);
            conn.addClient(client_ip, client_port, meta_data.second, meta_data.first);
            Conn::ClientState* clientState = conn.getClientState(client_ip, client_port);
            std::filesystem::path metaPath(meta_data.second);
            // normalize the path by removing any leading slashes before appending
            metaPath = metaPath.is_absolute() ? metaPath.relative_path() : metaPath;
            // combine with root path
            clientState->filePath = (std::filesystem::path(root_path) / metaPath).lexically_normal();

            //std::cout<<"Client using filepath: "<<clientState->filePath<<std::endl; //debugging
            auto it = active_files.find(clientState->filePath);
            if (it != active_files.end()) {
                if (it->second != client_port) {
                    ::write_busy(socket_p, client_addr, 0, addr_len);
                    continue;
                }
                ::server_send_ack(socket_p, client_addr, 0, addr_len, droppc); // client reestablished connection
                clientState->base_seq_num += 1;
                clientState->expected_seq_num += 1; //starting from 1 (META always 0)
            }else{ //client that has just started file writing
                clientState->base_seq_num += 1;
                clientState->expected_seq_num += 1; //starting from 1 (META always 0)
                clientState->fileStream = file_handle.open_file_stream(clientState->filePath);
                active_files[clientState->filePath] = client_port;
                ::server_send_ack(socket_p, client_addr, 0, addr_len, droppc); // send ack for meta (sequence num is 0)
                if (!clientState->fileStream->is_open()) {
                    active_files.erase(clientState->filePath);  // Corrected erasure
                    clientState->fileStream.reset();
                }
            }
            continue;
        }
        
        //data packets: [0]:flag; [1-4]:seq_num; [5-end]: data-body
        Conn::ClientState* clientState = conn.getClientState(client_ip, client_port);
        uint32_t seq_num = dgram.decode_bytes(std::vector<uint8_t>(buffer.begin() + 1, buffer.begin() + 5));
        
        if(should_drop()){
            ::logPacketEvent("DROP DATA", socket_p, client_addr, seq_num);
            continue;
        }

        ::logPacketEvent("DATA", socket_p, client_addr, seq_num);

        if(buffer[0] == FIN_FLAG){
            uint32_t fin_seq_num = dgram.decode_fin_packet(buffer);
            ::server_send_ack(socket_p, client_addr, fin_seq_num, addr_len, droppc);
            ::write_window(clientState);
            active_files.erase(clientState->filePath);  // Corrected erasure
            conn.removeClient(client_ip, client_port);
            continue;
        }
        
        if(buffer[0] == DATA_FLAG){
            // if from prev window ack
            if(seq_num < clientState->base_seq_num){
                ::server_send_ack(socket_p, client_addr, seq_num, addr_len, droppc);
                continue;
            }

            // if seq_num is within window range insert
            if(seq_num < clientState->base_seq_num + clientState->winSize){
                clientState->buffer[seq_num] = std::vector<uint8_t>(buffer.begin() + 5, buffer.end());
                ::server_send_ack(socket_p, client_addr, seq_num, addr_len, droppc);
            }

            // only writes if seq_num is base of sliding window
            if(seq_num == clientState->base_seq_num){
                ::write_window(clientState);
            }
        }
    }
}
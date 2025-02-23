#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdint>
#include <fstream>
#include <map>         
#include <sys/stat.h>
#include <limits.h>
#include <bitset>
#include <chrono>
#include <tuple>

#include "../../include/client/client.h"
#include "../../include/common/datagram.h"
#include "../../include/common/file_t.h"

#define STAN_SIZE 5 // standard size for ACK packets
#define MAX_RETRANSMISSIONS 5
#define MAX_WAIT_TIME_MS 30000
#define RETRY_INTERVAL_MS 2000

#define META_FLAG 0
#define DATA_FLAG 1
#define ACK_FLAG 2
#define FIN_FLAG 3
#define FILE_LOCK_FLAG 4 // termination flag client trying to write to already processed file

Dgram dgram;
File file_handle;

uint32_t seq_num = 1;
uint32_t base_seq = seq_num;
uint32_t next_seq = seq_num + 1;

struct timeval timeout;
struct timeval select_timeout;

std::map<uint32_t, std::vector<uint8_t>> packet_window; // Stores packets without timestamps

Client::Client() : socket_p(-1){};

Client::~Client(){
    if(socket_p >= 0) close(socket_p);
}

namespace{

    void initialize_timeouts(){
        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
    
        select_timeout.tv_sec = 3;
        select_timeout.tv_usec = 0;
    }

    void increment(uint32_t& x, uint32_t& y){
        x++;
        y++;
    }
}
namespace Log{
    void logPacketEvent(const std::string& type, uint32_t pkt_sn, uint32_t win){
        // Get current timestamp in RFC 3339 format
        std::time_t now = std::time(nullptr);
        std::tm* utcTime = std::gmtime(&now); 
    
        char buffer[30];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000Z", utcTime);
        std::string timestamp(buffer);
    
        // Prepare log entry
        std::string log_entry = timestamp + ", " + type + ", " + std::to_string(pkt_sn) + ", " +
                                std::to_string(base_seq) + ", " + std::to_string(next_seq) + ", " +
                                std::to_string(base_seq + win) + "\n";
    
        std::cout << log_entry;
    }
}

namespace Packet_Handle{
    // stop and wait used for META and FIN packets
    int wait_for_response(int socket_fds,
        struct sockaddr_in &srv_addr, 
        Client::PacketInfo& packet_info){

        fd_set read_fds;
        std::vector<uint8_t> send_packet = packet_info.packet.data_body;
        //std::chrono::steady_clock::time_point sent_time = packet_info.sent_time; // for debugging
        std::vector<uint8_t> buffer(STAN_SIZE);

        while(packet_info.transmission < MAX_RETRANSMISSIONS){ // only a max of 5 transmission attempts
            FD_ZERO(&read_fds);
            FD_SET(socket_fds, &read_fds);
            send(socket_fds, send_packet.data(), send_packet.size(), 0);
            int active = select(socket_fds + 1, &read_fds, nullptr, nullptr, &select_timeout);
            if(active > 0 && FD_ISSET(socket_fds, &read_fds)){
                int bytes_read = recv(socket_fds, buffer.data(), buffer.size(), 0);
                if(bytes_read < 0){
                    if(errno == ECONNREFUSED){ // TODO: reconnect to server? Add functionality
                        std::cerr<<"Interrupted -- Server Offline"<<std::endl;
                        close(socket_fds);
                        exit(5); // TODO: actual exit status
                    }
                }else{
                    return buffer[0];
                }
            }
            std::cout<<"Retransmitting "<<packet_info.transmission<<"/5"<<std::endl; //debugging
            packet_info.transmission++;
        }
        std::cerr<<"Max Retransmission attempts reached"<<std::endl;
        close(socket_fds);
        exit(4);
    }

    int meta_packet_handle(int socket_fds, 
                        struct sockaddr_in &srv_addr, 
                        uint32_t winsz,
                        const std::string &output_file){
        
        std::vector<uint8_t> _meta_packet = dgram.encode_meta_packet(winsz, output_file);
        Client::packet_t meta_packet = {0, _meta_packet};
        Client::PacketInfo m_packet = {meta_packet, std::chrono::steady_clock::now()};

        bool start_timeout = false;
        while(!m_packet.acked){
            int p_flag = wait_for_response(socket_fds, srv_addr, m_packet); // response flag for meta packet (file could already be in use)
            if(p_flag == ACK_FLAG){
                m_packet.acked = true;
                std::cout << "META packet acknowledged. Starting data transfer...\n";
            }else if(p_flag == FILE_LOCK_FLAG){ // file already in use --> wait
                if(!start_timeout){ //start timeout period (first instance of flag)
                    m_packet.sent_time = std::chrono::steady_clock::now(); // update to have accurate timeout
                    start_timeout = true;
                }
                std::cout << "File is in use, retrying...\n";
                auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - m_packet.sent_time)
                                        .count();
                if(elapsed_time >= MAX_WAIT_TIME_MS){
                    std::cerr << "File lock timeout reached (30s). Exiting with code 6.\n";
                    close(socket_fds);
                    exit(6);
                }
                usleep(RETRY_INTERVAL_MS * 1000);  // Wait before retrying
            }
        }
        return 0;
    }

    int data_packet_handle(int socket_fds,
                        struct sockaddr_in &srv_addr,
                        std::string filePath,
                        int win,
                        int mss){
        std::map<uint32_t, Client::PacketInfo> window;

        fd_set read_fds;
        fd_set write_fds;
        int read_pos;
        while (true) {
            FD_ZERO(&read_fds);
            FD_ZERO(&write_fds);
            FD_SET(socket_fds, &read_fds);
            FD_SET(socket_fds, &write_fds);
    
            int activity = select(socket_fds + 1, &read_fds, &write_fds, nullptr, &select_timeout);
            if(activity < 0){
                std::cerr << "Select error\n";
                break;
            }
    
            if(FD_ISSET(socket_fds, &write_fds) && static_cast<int>(window.size()) < win){ // writes: sends packets to window and server
                std::pair<std::vector<uint8_t>, int> file_info = file_handle.file_read_stream(filePath, mss, read_pos);
                if(file_info.first.empty()){ //EOF
                    FD_CLR(socket_fds, &write_fds);
                    std::cout<<"END OF FILE REACHED"<<std::endl;
                    break;
                }
                read_pos = read_pos + file_info.second;
                Client::packet_t data_packet = {seq_num, dgram.encode_data_packet(seq_num, file_info.first)};
                Client::PacketInfo packet_info = {data_packet, std::chrono::steady_clock::now()}; //packet, sent_time
                window.insert(std::make_pair(seq_num, packet_info));
                ::increment(seq_num, next_seq);
                send(socket_fds, data_packet.data_body.data(), data_packet.data_body.size(), 0);
            }
    
            if(FD_ISSET(socket_fds, &read_fds)){ // reads: checks for acks
                std::vector<uint8_t> ack_packet(STAN_SIZE);
                int bytes_read = recv(socket_fds, ack_packet.data(), ack_packet.size(), 0);
                if(bytes_read < 0){
                    if(errno == ECONNREFUSED){ // TODO: reconnect to server? Add functionality
                        std::cerr<<"Interrupted -- Server Offline"<<std::endl;
                        close(socket_fds);
                        exit(5); // TODO: actual exit status
                    }
                }

                uint32_t ack_seq = dgram.decode_ack_packet(ack_packet); // returns sequence number of packet acked
                if(window.find(ack_seq) != window.end()){
                    window[ack_seq].acked = true;
                }
                while(!window.empty() && window.begin()->second.acked){
                    window.erase(window.begin());
                    base_seq++;
                }
            }
    
            auto now = std::chrono::steady_clock::now();
            for(auto &[snum, packet_info] : window){
                if(!packet_info.acked && std::chrono::duration_cast<std::chrono::seconds>(now - packet_info.sent_time).count() > MAX_WAIT_TIME_MS){
                    std::cerr<<"Packet Loss Detected"<<std::endl;
                    if(packet_info.transmission > MAX_RETRANSMISSIONS){
                        std::cerr<<"Max Retransmission attempts reached"<<std::endl;
                        close(socket_fds);
                        exit(4);
                    }
                    send(socket_fds, packet_info.packet.data_body.data(), packet_info.packet.data_body.size(), 0);
                    packet_info.sent_time = now;
                    packet_info.transmission += 1;
                    std::cout << "Retransmitting seq " << snum << "\n";
                }
            }
        }
        return 0;
    }

    int fin_packet_handle(int socket_fds, 
                        struct sockaddr_in &srv_addr){
        std::vector<uint8_t> _fin_packet = dgram.encode_fin_packet(seq_num);
        Client::packet_t fin_packet = {seq_num, _fin_packet};
        Client::PacketInfo f_packet = {fin_packet, std::chrono::steady_clock::now()};
        if(wait_for_response(socket_fds, srv_addr, f_packet) != ACK_FLAG){
            std::cerr<<"Unexpected FIN packet response from Server"<<std::endl;
            return -1;
        };
        std::cout<<"Closing connection to server"<<std::endl;
        return 0;
    }

}

void Client::socket_init(){
    socket_p = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_p < 0){
        throw std::runtime_error(std::string("socket initialization failed: ") + std::string(strerror(errno)));
    }
    if(setsockopt(socket_p, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
        close(socket_p);
        throw std::runtime_error(std::string("Failed to set socket timeout: ") + std::string(strerror(errno)));
    }
    if(setsockopt(socket_p, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0){
        close(socket_p);
        throw std::runtime_error("Failed to set send timeout: " + std::string(strerror(errno)));
    }
}

void Client::client_communicate(const char *srv_ip, 
                                const int &srv_port, 
                                const uint32_t &winsz, 
                                const int &mss, 
                                const std::string &input_file,
                                const std::string &output_file){
    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(srv_port);
    ::initialize_timeouts();

    if(inet_pton(AF_INET, srv_ip, &srv_addr.sin_addr) <= 0){
        close(socket_p);
        throw std::runtime_error("Invalid server IP address");
    }

    if(connect(socket_p, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0){
        close(socket_p);
        std::cerr << "Failed to connect socket\n";
        exit(1);
    }

    struct timeval recv_timeout = {30, 0}; 
    if (setsockopt(socket_p, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0) {
        std::cerr << "Error setting recv timeout: " << strerror(errno) << std::endl;
        exit(1);
    }

    // // sendto timeout 10 seconds
    // struct timeval send_timeout = {10, 0};  
    // if (setsockopt(socket_p, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout)) < 0) {
    //     std::cerr << "Error setting send timeout: " << strerror(errno) << std::endl;
    //     exit(1);
    // }

    // META Packets
    Packet_Handle::meta_packet_handle(socket_p, srv_addr, winsz, output_file);
    
    // handling DATA Packets
    Packet_Handle::data_packet_handle(socket_p, srv_addr, input_file, winsz, mss);

    // FIN Packets 
    Packet_Handle::fin_packet_handle(socket_p, srv_addr);
}
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
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <mutex>

#include "../../include/client/client.h"
#include "../../include/common/datagram.h"
#include "../../include/common/file_t.h"

#define STAN_SIZE 5 // standard size for ACK packets
#define MAX_RETRANSMISSIONS 5
#define MAX_WAIT_TIME_S 3
#define RETRY_INTERVAL_MS 2000
#define MAX_FILE_WAIT_TIME 30

#define META_FLAG 0
#define DATA_FLAG 1
#define ACK_FLAG 2
#define FIN_FLAG 3
#define FILE_LOCK_FLAG 9 // termination flag client trying to write to already processed file

Dgram dgram;
File file_handle;

thread_local uint32_t seq_num = 0;
thread_local uint32_t base_seq = seq_num;
thread_local uint32_t next_seq = seq_num + 1;

thread_local uint32_t win = 0;

thread_local struct timeval timeout;

thread_local std::map<uint32_t, std::vector<uint8_t>> packet_window; // stores packets without timestamps

std::mutex g_print_mutex;

Client::Client() : socket_p(-1){};

Client::~Client() {
    if (socket_p >= 0) {
        std::cout << "Thread " << std::this_thread::get_id() 
                  << " closing SOCKET_P: " << socket_p << std::endl;
        shutdown(socket_p, SHUT_RDWR); 
        close(socket_p);
        socket_p = -1;
    }
}

namespace{

    void safePrintErr(const std::string& message) {
        std::lock_guard<std::mutex> lock(g_print_mutex);
        std::cerr << message << std::endl;
    }
    
    void initialize_timeouts(){
        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
    }

    void increment(uint32_t& x, uint32_t& y){
        x++;
        y++;
    }
}
namespace Log{
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
                        uint32_t pkt_sn
                    ){

        uint16_t lport = Log::getLocalPort(socket_fds);
        uint16_t rport = ntohs(srv_addr.sin_port);
        std::string rip = static_cast<std::string>(inet_ntoa(srv_addr.sin_addr));
        // get current timestamp in RFC 3339 format
        std::time_t now = std::time(nullptr);
        std::tm* utcTime = std::gmtime(&now);

        std::ostringstream timestamp;
        timestamp << std::put_time(utcTime, "%Y-%m-%dT%H:%M:%S") << ".000Z";

        std::string logEntry = timestamp.str() + ", " 
        + std::to_string(lport) + ", " + rip + ", " + std::to_string(rport) + ", " 
        + type + ", " + std::to_string(pkt_sn) + ", " + std::to_string(base_seq) + ", " + std::to_string(next_seq) + ", " 
        + std::to_string(base_seq + win) + "\n";

        std::cout << logEntry;

        //append to file for graph creation
        std::ofstream logStream("logFile.txt", std::ios::app);
        if (logStream.is_open()) {
            logStream << logEntry;
            logStream.close();
        } else {
            std::cerr << "Error opening log file: " << "logFile.txt" << std::endl;
        }
    }
}

namespace Packet_Handle{
    // used for fin/meta packest
    int wait_for_response(int socket_fds,
                        struct sockaddr_in &srv_addr, 
                        Client::PacketInfo& packet_info){
        fd_set read_fds;
        std::vector<uint8_t> send_packet = packet_info.packet.data_body;
        std::vector<uint8_t> buffer(STAN_SIZE);
        char ip_str[INET_ADDRSTRLEN];

        while(packet_info.transmission < MAX_RETRANSMISSIONS){ // max 5 attempts
            FD_ZERO(&read_fds);
            FD_SET(socket_fds, &read_fds);
            // Send the packet and log the event.
            ssize_t sent_bytes = send(socket_fds, send_packet.data(), send_packet.size(), 0);
            if(sent_bytes < 0){
                std::ostringstream oss;
                oss << "Send failed on socket " << socket_fds << " (" << std::strerror(errno) << ")";
                ::safePrintErr(oss.str());
                return 5;
            }
            Log::logPacketEvent("DATA", socket_fds, srv_addr, packet_info.packet.seq_num);

            // Set a 3-second timeout for select().
            struct timeval select_timeout;
            select_timeout.tv_sec = 3;
            select_timeout.tv_usec = 0;

            int active = select(socket_fds + 1, &read_fds, nullptr, nullptr, &select_timeout);
            if(active < 0){
                std::ostringstream oss;
                oss << "Select error on socket " << socket_fds << " (" << std::strerror(errno) << ")";
                ::safePrintErr(oss.str());
                return 1;
            }

            if(active > 0 && FD_ISSET(socket_fds, &read_fds)){
                int bytes_read = recv(socket_fds, buffer.data(), buffer.size(), 0);
                if(bytes_read > 0){
                    std::pair<uint8_t, uint32_t> resp = dgram.decode_response(buffer);
                    if(resp.second == 0){ //response to meta packet
                        return resp.first; // received response (could be ACK or FILE_LOCK)
                    }
                    Log::logPacketEvent("ACK", socket_fds, srv_addr, resp.second); // else for fin packets
                    return ACK_FLAG; // expected ACK_FLAG response
                }else if (bytes_read == 0){
                    // Connection closed by peer.
                    inet_ntop(AF_INET, &(srv_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                    std::ostringstream oss;
                    oss << "Connection closed by server IP " << ip_str;
                    ::safePrintErr(oss.str());
                    return 5;
                }else if (errno == ECONNREFUSED){ 
                    inet_ntop(AF_INET, &(srv_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                    std::ostringstream oss;
                    oss << "Server is down IP " << ip_str 
                        << " port " << ntohs(srv_addr.sin_port)
                        << " (" << std::strerror(errno) << ")";
                    ::safePrintErr(oss.str());
                    return 5;
                }else{
                    // Other recv error.
                    std::ostringstream oss;
                    oss << "Recv error on socket " << socket_fds 
                        << " (" << std::strerror(errno) << ")";
                    ::safePrintErr(oss.str());
                    return 1;
                }
            }

            {
                std::ostringstream oss;
                oss << "Packet loss detected, retransmitting after 3 seconds...";
                ::safePrintErr(oss.str());
            }
            packet_info.transmission++;
        }

        // Exceeded the maximum retransmission limit.
        inet_ntop(AF_INET, &(srv_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
        {
            std::ostringstream oss;
            oss << "Reached max re-transmission limit IP " << ip_str;
            ::safePrintErr(oss.str());
        }
        return 4;
    }


int meta_packet_handle(int socket_fds, 
                        struct sockaddr_in &srv_addr, 
                        uint32_t winsz,
                        const std::string &output_file){

    // encode meta packet and create PacketInfo.
    std::vector<uint8_t> _meta_packet = dgram.encode_meta_packet(winsz, output_file);
    Client::packet_t meta_packet(0, _meta_packet);
    Client::PacketInfo m_packet(meta_packet, std::chrono::steady_clock::now());

    bool start_timeout = false;
    
    while (!m_packet.acked) {
        int p_flag = Packet_Handle::wait_for_response(socket_fds, srv_addr, m_packet); // response flag for meta packet

        if(p_flag == ACK_FLAG){  
            // ack received for meta packet
            Log::logPacketEvent("ACK", socket_fds, srv_addr, 0);
            return 0;
        }
        if(p_flag == FILE_LOCK_FLAG){  
            // start file lock timeout
            if(!start_timeout){  
                m_packet.sent_time = std::chrono::steady_clock::now();
                start_timeout = true;
            }
            {
                std::ostringstream oss;
                oss << "File is in use, retrying...";
                ::safePrintErr(oss.str());
            }
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::steady_clock::now() - m_packet.sent_time)
                                    .count();
            if(elapsed_time >= MAX_FILE_WAIT_TIME){
                std::ostringstream oss;
                oss << "File lock timeout reached (30s). Exiting with code 20.";
                ::safePrintErr(oss.str());
                return 20; 
            }
            usleep(RETRY_INTERVAL_MS * 1000);
            continue;
        } 
        if(p_flag == 4){  
            return 4;  
        }
        return p_flag;
    }

    return 1;
}


    int data_packet_handle(int socket_fds,
                        struct sockaddr_in &srv_addr,
                        std::string filePath,
                        int win,
                        int mss)
                        {
        std::map<uint32_t, Client::PacketInfo> window;
        fd_set read_fds;
        fd_set write_fds;
        int read_pos = 0;
        bool eofReached = false;  // Flag to mark that we've reached end-of-file

        while (true) {
            FD_ZERO(&read_fds);
            FD_ZERO(&write_fds);
            FD_SET(socket_fds, &read_fds);
            FD_SET(socket_fds, &write_fds);

            // Set a short timeout (2 seconds) for periodic retransmission checks.
            struct timeval short_timeout;
            short_timeout.tv_sec = 2;
            short_timeout.tv_usec = 0;

            int activity = select(socket_fds + 1, &read_fds, &write_fds, nullptr, &short_timeout);
            if(activity < 0){
                std::ostringstream oss;
                oss << "Select error on socket " << socket_fds;
                ::safePrintErr(oss.str());
                return 7;
            }

            // Only try to read from the file (send new packets) if we haven't hit EOF.
            if(!eofReached && activity > 0 &&
                FD_ISSET(socket_fds, &write_fds) &&
                static_cast<int>(window.size()) < win){

                auto file_info = file_handle.file_read_stream(filePath, mss, read_pos);
                if(file_info.first.empty()){ // EOF reached
                    eofReached = true;
                    // std::ostringstream oss; // debugging 
                    // oss << "End of file reached for file: " << filePath;
                    // ::safePrintErr(oss.str());
                }else{
                    read_pos += file_info.second;
                    Client::packet_t data_packet(seq_num, dgram.encode_data_packet(seq_num, file_info.first));
                    Client::PacketInfo packet_info(data_packet, std::chrono::steady_clock::now());
                    window.insert({seq_num, packet_info});
                    increment(seq_num, next_seq);
                    ssize_t sent_bytes = send(socket_fds, data_packet.data_body.data(), data_packet.data_body.size(), 0);
                    if(sent_bytes < 0){
                        std::ostringstream oss;
                        oss << "Send error on socket " << socket_fds 
                            << " (" << std::strerror(errno) << ")";
                        ::safePrintErr(oss.str());
                    }
                    Log::logPacketEvent("DATA", socket_fds, srv_addr, packet_info.packet.seq_num);
                }
            }

            // Handle incoming ACKs.
            if(activity > 0 && FD_ISSET(socket_fds, &read_fds)){
                std::vector<uint8_t> ack_packet(STAN_SIZE);
                int bytes_read = recv(socket_fds, ack_packet.data(), ack_packet.size(), 0);
                if(bytes_read < 0){
                    if (errno == ECONNREFUSED) {
                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(srv_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                        std::ostringstream oss;
                        oss << "Server is down IP " << ip_str 
                            << " port " << ntohs(srv_addr.sin_port);
                        ::safePrintErr(oss.str());
                        return 3;
                    }
                }
                uint32_t ack_seq = dgram.decode_ack_packet(ack_packet);
                Log::logPacketEvent("ACK", socket_fds, srv_addr, ack_seq);
                if(window.find(ack_seq) != window.end()){
                    window[ack_seq].acked = true;
                }
                // Remove all consecutively acknowledged packets from the window.
                while(!window.empty() && window.begin()->second.acked){
                    window.erase(window.begin());
                    base_seq++;  // Update base_seq as needed.
                }
            }

            // Check for expired packets and retransmit if necessary.
            auto now = std::chrono::steady_clock::now();
            for(auto it = window.begin(); it != window.end(); ){
                auto &[snum, packet_info] = *it;
                if(!packet_info.acked &&
                    std::chrono::duration_cast<std::chrono::seconds>(now - packet_info.sent_time).count() > MAX_WAIT_TIME_S)
                {
                    if(packet_info.transmission > MAX_RETRANSMISSIONS){
                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(srv_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                        std::ostringstream oss;
                        oss << "Reached max re-transmission limit IP " << ip_str;
                        ::safePrintErr(oss.str());
                        return 4;
                    }
                    ssize_t sent_bytes = send(socket_fds, packet_info.packet.data_body.data(),
                                            packet_info.packet.data_body.size(), 0);
                    if(sent_bytes < 0){
                        std::ostringstream oss;
                        oss << "Retransmit send error on socket " << socket_fds 
                            << " (" << std::strerror(errno) << ")";
                        ::safePrintErr(oss.str());
                    }
                    packet_info.sent_time = now;
                    packet_info.transmission += 1;
                }
                ++it;
            }

            // ff we reached EOF and the window is empty --> finish
            if(eofReached && window.empty()){
                break;
            }
        }
        return 0;
    }



    int fin_packet_handle(int socket_fds, struct sockaddr_in &srv_addr){
        std::vector<uint8_t> _fin_packet = dgram.encode_fin_packet(seq_num);
        Client::packet_t fin_packet(seq_num, _fin_packet);
        Client::PacketInfo f_packet(fin_packet, std::chrono::steady_clock::now());

        if(wait_for_response(socket_fds, srv_addr, f_packet) != ACK_FLAG){
            std::ostringstream oss;
            oss << "Unexpected FIN packet response from Server";
            ::safePrintErr(oss.str());
            return 1;
        }
        {
            std::ostringstream oss;
            oss << "Closing connection to server";
            ::safePrintErr(oss.str());
        }
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
    std::cout << "Thread " <<std::this_thread::get_id() << " Client Instance: "<< this << " SOCKET_P: " <<socket_p <<std::endl;
}

int Client::client_communicate(const char* srv_ip,
                                const int &srv_port,
                                const uint32_t &winsz,
                                const int &mss,
                                const std::string &input_file,
                                const std::string &output_file) {
    win = winsz;

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(srv_port);
    ::initialize_timeouts();

    //std::cout<<"SRV IP "<<srv_ip<<std::endl; // debugging
    if(inet_pton(AF_INET, srv_ip, &srv_addr.sin_addr) <= 0){
        std::cerr << "Invalid server IP address" << std::endl;
        return 1;
    }

    if(connect(socket_p, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0){
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(srv_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
        std::ostringstream oss;
        oss << "Cannot detect server IP " << ip_str << " port " << ntohs(srv_addr.sin_port);
        ::safePrintErr(oss.str());
        return 3;
    }

    struct timeval recv_timeout = {30, 0};
    if(setsockopt(socket_p, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0){
        std::ostringstream oss;
        oss << "Error setting recv timeout: " << strerror(errno);
        ::safePrintErr(oss.str());
        return 1;
    }

    struct timeval send_timeout = {10, 0};
    if(setsockopt(socket_p, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout)) < 0){
        std::ostringstream oss;
        oss << "Error setting send timeout: " << strerror(errno);
        ::safePrintErr(oss.str());
        return 1;
    }

    int err = 0;
    // META Packets.
    err = Packet_Handle::meta_packet_handle(socket_p, srv_addr, winsz, output_file);
    if(err != 0){
        shutdown(socket_p, SHUT_RDWR);
        close(socket_p);
        return err;
    }

    ::increment(seq_num, base_seq);

    // DATA Packets.
    err = Packet_Handle::data_packet_handle(socket_p, srv_addr, input_file, winsz, mss);
    if(err != 0){
        shutdown(socket_p, SHUT_RDWR);
        close(socket_p);
        return err;
    }
    // FIN Packets.
    err = Packet_Handle::fin_packet_handle(socket_p, srv_addr);
    if(err != 0){
        shutdown(socket_p, SHUT_RDWR);
        close(socket_p);
        return err;
    }

    // std::cout << "Thread " << std::this_thread::get_id() << " closing SOCKET_P: " << socket_p << std::endl; // debugging
    socket_p = -1;  // Mark as closed to prevent accidental reuse
    return 0;
}
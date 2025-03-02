#ifndef CLIENT
#define CLIENT

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <chrono>

#include <sys/socket.h>
#include <netinet/in.h>

class Client{
private:
    int socket_p;
public:
    Client();
    ~Client();

    struct client_info{ //used for thread safety in main()
        char* ip_str;
        int port_num;
        uint32_t winsz;
        int mss;
        std::string input_file;
        std::string output_file;
    };

    struct packet_t{ // client package datagram
        uint32_t seq_num;
        std::vector<uint8_t> data_body;

        packet_t(uint32_t seq, const std::vector<uint8_t>& data)
            : seq_num(seq), data_body(data) {}
    };

    struct PacketInfo{
        packet_t packet;
        std::chrono::steady_clock::time_point sent_time;
        bool acked = false;
        int transmission = 0;

        PacketInfo() : packet(0, std::vector<uint8_t>()){}  
        PacketInfo(const packet_t& pkt, std::chrono::steady_clock::time_point time)
        : packet(pkt), sent_time(time) {}
    };

    void socket_init();
    int client_communicate(const char*,
                    const int&,
                    const uint32_t&,
                    const int&,
                    const std::string&,
                    const std::string&);
};
#endif
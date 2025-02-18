#include <cstdlib>
#include <string>
#include <cstring>
#include <map>

#include "../../include/server/conn.h"

Conn::Conn() {}

Conn::~Conn() {
    clients.clear();
}

void Conn::addClient(const std::string& ip, uint16_t port, const std::string& filePath, uint32_t winSize){
    auto clientKey = std::make_pair(ip, port);
    if (clients.find(clientKey) == clients.end()) {
        clients[clientKey] = ClientState(filePath, winSize);
    }
}

bool Conn::clientExists(const std::string& ip, uint16_t port) const{
    auto clientKey = std::make_pair(ip, port);
    return clients.find(clientKey) != clients.end();
}

// return const ClientState struct for specific client
const Conn::ClientState& Conn::getClientState(const std::string& ip, uint16_t port) const{
    auto clientKey = std::make_pair(ip, port);
    auto check = clients.find(clientKey);
    static const ClientState emptyState;
    return (check != clients.end()) ? check->second : emptyState;
}

// pass by value
Conn::ClientState& Conn::getClientState(const std::string& ip, uint16_t port){
    auto clientKey = std::make_pair(ip, port);
    auto check = clients.find(clientKey);
    static ClientState emptyState;
    return (check != clients.end()) ? check->second : emptyState;
}

void Conn::removeClient(const std::string& ip, uint16_t port){
    auto clientKey = std::make_pair(ip, port);
    clients.erase(clientKey);
}

const std::string Conn:getClientFile(const std::string& ip, uint16_t port){
    ClientState client = getClientState(ip, port);
    return client.filePath;
}
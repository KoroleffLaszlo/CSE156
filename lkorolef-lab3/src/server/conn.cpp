#include <cstdlib>
#include <string>
#include <cstring>

#include "../include/Conn.h"

Conn::Conn() {}

Conn::~Conn() {
    clients.clear();
}

void Conn::addClient(const std::string& ip, uint16_t port, const std::string& filePath, uint16_t winSize) {
    auto clientKey = std::make_pair(ip, port);
    if (clients.find(clientKey) == clients.end()) {
        clients[clientKey] = ClientState(filePath, winSize);
    }
}

bool Conn::clientExists(const std::string& ip, uint16_t port) const {
    auto clientKey = std::make_pair(ip, port);
    return clients.find(clientKey) != clients.end();
}

const Conn::ClientState* Conn::getClientState(const std::string& ip, uint16_t port) const{
    auto clientKey = std::make_pair(ip, port);
    auto check = clients.find(clientKey);
    return (check != clients.end()) ? &check->second : nullptr;
}

Conn::ClientState* Conn::getClientState(const std::string& ip, uint16_t port) {
    auto clientKey = std::make_pair(ip, port);
    auto check = clients.find(clientKey);
    return (check != clients.end()) ? &check->second : nullptr;
}

void Conn::removeClient(const std::string& ip, uint16_t port) {
    auto clientKey = std::make_pair(ip, port);
    clients.erase(clientKey);
}
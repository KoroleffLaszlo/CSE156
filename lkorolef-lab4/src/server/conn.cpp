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

Conn::ClientState* Conn::getClientState(const std::string& ip, uint16_t port){
    auto clientKey = std::make_pair(ip, port);
    auto check = clients.find(clientKey);
    if(check != clients.end()){
        return &check->second; // return pointer to existing client state
    }
    return nullptr; // client not found
}

void Conn::removeClient(const std::string& ip, uint16_t port){
    auto clientKey = std::make_pair(ip, port);
    clients.erase(clientKey);
}

std::string Conn::getClientFile(const std::string& ip, uint16_t port){
    ClientState *client = getClientState(ip, port);
    return client->filePath;
}
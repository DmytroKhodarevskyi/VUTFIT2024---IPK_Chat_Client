#include "udp.h"

Udp::Udp(sockaddr_in server_address, int sock) 
    : server_address(server_address), sock(sock) {}


void Udp::sendMessage(const std::string& message) {
    // Send a message to the server
    const char* message_cstr = message.c_str();
    if (send(sock, message_cstr, strlen(message_cstr), 0) == -1) {
        std::cerr << "Could not send message\n";
        close(sock);
        exit(3);
    }
}

void Udp::closeConnection() {
    close(sock);
}

void Udp::Input() {
    // std::string message;
    // std::cout << "Enter a message: ";
    // std::getline(std::cin, message);
    // sendMessage(message);
    std::string line;

    while (std::getline(std::cin, line)) {
        std::cout << "Read line: " << line << std::endl;
    }
}
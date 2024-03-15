#ifndef UDP_H
#define UDP_H

#include <unistd.h> // For close
#include <iostream>
#include <string.h> // For strlen
#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For sockaddr_in

class Udp {
    public:   
        // Tcp(const std::string& serverIP, unsigned short serverPort, int sock);
        Udp(sockaddr_in server_address, int sock);
        // void connectToServer();
        void sendMessage(const std::string& message);
        void closeConnection();
        void Input();

    private:
        struct sockaddr_in server_address;
        // std::string serverIP;
        // unsigned short serverPort;
        int sock;

};

#endif // UDP_H

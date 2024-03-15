#ifndef TCP_H
#define TCP_H

#include <unistd.h> // For close
#include <iostream>
#include <string.h> // For strlen
#include <sys/socket.h> // For socket functions
#include <sys/select.h> // For select
#include <netinet/in.h> // For sockaddr_in

#include "input_process.h"

using namespace std;

class Tcp {
    public:   
        // Tcp(const string& serverIP, unsigned short serverPort, int sock);
        Tcp(sockaddr_in server_address, int sock);
        void connectToServer();
        void sendMessage(const string& message);
        string receiveMessage();
        void closeConnection();

        void stdinReaderThread(InputBuffer& inputBuffer);
        void printBuffer(InputBuffer& inputBuffer);
        void networkCommunicationThread(InputBuffer& inputBuffer);

        void Input();

    private:
        struct sockaddr_in server_address;
        // string serverIP;
        // unsigned short serverPort;
        int sock;

        string ERR_ = "ERR: ";

        enum Status {
            START,
            AUTH,
            OPEN,
            ERR,
            END
        } Status;

};

#endif // TCP_H

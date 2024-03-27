// #ifndef UDP_H
// #define UDP_H

// #include <unistd.h> // For close
// #include <iostream>
// #include <string.h> // For strlen
// #include <sys/socket.h> // For socket functions
// #include <sys/select.h> // For select
// #include <netinet/in.h> // For sockaddr_in

// #include "input_process.h"


// class Udp {
//     public:   
//         // Tcp(const std::string& serverIP, unsigned short serverPort, int sock);
//         Udp(sockaddr_in server_address, int sock);
//         // void connectToServer();
//         void sendMessage(const std::string& message);
//         void closeConnection();
//         void Input();

//         void stdinReaderThread(InputBuffer& inputBuffer);
//         void printBuffer(InputBuffer& inputBuffer);
//         void networkCommunicationThread(InputBuffer& inputBuffer);

//     private:
//         struct sockaddr_in server_address;
//         // std::string serverIP;
//         // unsigned short serverPort;
//         int sock;

// };

// #endif // UDP_H

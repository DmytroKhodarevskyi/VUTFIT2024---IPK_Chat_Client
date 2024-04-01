#ifndef TCP_UDP_H
#define TCP_UDP_H

#include <unistd.h> // For close
#include <iostream>
#include <string.h> // For strlen
#include <sys/select.h> // For select
#include <sys/types.h>
#include <fcntl.h>
#include <iomanip>

#include <csignal>
#include <set>

#include <tuple>

#include <memory> // For std::unique_ptr
#include <vector>

#include <mutex>
#include <condition_variable>
#include <chrono>

#include "memory_manager.h"

#include <netinet/in.h> // For sockaddr_in

#include <cstring> // For memset() and strerror()
#include <cerrno>  // For errno
#include <sys/socket.h>
#include <arpa/inet.h>

#include <cerrno> // Include errno

#include "input_process.h"
#include "response_buffer.h"

using namespace std;

class TcpUdp {
    public:   
        TcpUdp(sockaddr_in server_address, int sock, unsigned short UdpTimeout, unsigned short UdpRetransmissions);
        void Input(int Protocol);

        /**
         * @brief This function will connect you to the server when the TCP protocol is specified.
         */
        void connectToServer();

        /**
         * @brief This function will send a message for TCP protocol.
         * @param message The message to be sent to the server.
         */
        void sendMessageTCP(const string& message);

        /**
         * @brief This function will send a message for UDP protocol (and receive confirm).
         * @param message The message to be sent to the server.
         * @param length The length of the message to be sent to the server.
         */
        void sendMessageUDP(const char* message, size_t length);
        /**
         * @brief Sends confirmation to server.
         * @param ID Message ID.
         */
        void sendConfirm(short ID);
        /**
         * @brief Sends bye to server.
         * @param ID Message ID.
         */
        void sendByeUDP(short ID);

        /**
         * @brief This function will send ERR to server and gracefully end the program.
         */
        void handleErrFromServerUDP();
        
        string receiveMessageTCP();
        char* receiveMessageUDP(int *err, int *bytes);

        void closeConnection();

        void networkCommunicationThread();
        void stdinReaderThread();
        void confirmThread(short expectedID, const char* message, size_t length);
        void receiverThread();
        void receiverThreadTCP();
        void signalHandlerThread(const sigset_t& signals);

        /**
         * @brief This function is used for debug, you can print out the hex representation of a message.
         * @param message The message you want to print.
         * @param length The length of the message you want to print
         */
        void printMessageAsHex(const char* message, size_t length);
        void printBuffer(InputBuffer& inputBuffer);

        void setReceived(bool received);

    private:
        mutex sendingMutex;
        mutex bufferMutex;
        mutex confirmMutex;
        condition_variable confirmCondition;

        mutex confirmReceivedMutex;

        bool confirmReceived = false;

        MemoryManager memoryManager;
        ResponseBuffer responseBuffer;
        InputBuffer inputBuffer;
        struct sockaddr_in server_address;
        int sock;

        unsigned short UdpTimeout;
        unsigned short UdpRetransmissions;

        string ERR_ = "ERR: ";

        enum Status {
            START,
            AUTH,
            OPEN,
            ERR,
            END
        } Status;

        enum Status CurrentStatus = Status::START;

        unsigned short SentID = 0;
        unsigned short ReceivedID = 0;

    	string DisplayName = "NONE";

};

#endif // TCP_UDP_H

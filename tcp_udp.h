#ifndef TCP_UDP_H
#define TCP_UDP_H

#include <unistd.h> 
#include <iostream>
#include <string.h> 
#include <sys/select.h> 
#include <sys/types.h>
#include <fcntl.h>
#include <iomanip>

#include <csignal>
#include <set>

#include <tuple>

#include <memory> 
#include <vector>

#include <mutex>
#include <condition_variable>
#include <chrono>

#include <netinet/in.h> 

#include <cstring> 
#include <cerrno>  
#include <sys/socket.h>
#include <arpa/inet.h>

#include "input_process.h"
#include "response_buffer.h"

using namespace std;

class TcpUdp {
    public:   
        /**
         * @brief Constructor for the TcpUdp class, sets up the paramaters for network processing.
         */
        TcpUdp(sockaddr_in server_address, int sock, unsigned short UdpTimeout, unsigned short UdpRetransmissions);
        void Input(int Protocol);

    private:
        /**
         * @brief Connect you to the server when the TCP protocol is specified.
         * (connection parameters are specified in constructor)
         */
        void connectToServer();

        /**
         * @brief Send a message for TCP protocol.
         * @param message The message to be sent to the server.
         */
        void sendMessageTCP(const string& message);

        /**
         * @brief Send a message for UDP protocol (and receive confirm).
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
        
        /**
         * @brief This function will send ERR to server and gracefully end the program.
         * @return The message received from the server. Empty string if no message received.
         */
        string receiveMessageTCP();
        /**
         * @brief This function will receive a message from the server.
         * @param err Error code.
         * @param bytes Number of bytes received.
         * @return The message received from the server.
         */
        char* receiveMessageUDP(int *err, int *bytes);

        /**
         * @brief Thread for general network processing according to input.
         */        
        void networkCommunicationThread();
        /**
         * @brief Thread for reading the stdin.
         */  
        void stdinReaderThread();
        /**
         * @brief Thread for receiving the UDP confirmation.
         */  
        void confirmThread(short expectedID, const char* message, size_t length);
        /**
         * @brief Thread for receiving UDP messages from server.
         */
        void receiverThreadUDP();
        /**
         * @brief Thread for receiving TCP messages from server.
         */
        void receiverThreadTCP();
        /**
         * @brief Thread for receiving SIGINT.
         */
        void signalHandlerThread(const sigset_t& signals);

        /**
         * @brief This function is used for debug, you can print out the hex representation of a message.
         * @param message The message you want to print.
         * @param length The length of the message you want to print
         */
        void printMessageAsHex(const char* message, size_t length);
        /**
         * @brief This function is used for debug, you can print out the buffer contents.
         * @param inputBuffer Buffer you want to print.
         */
        void printBuffer(InputBuffer& inputBuffer);

        /**
         * @brief Setter for a receival flag (thread safe).
         * @param received received flag switch.
         */
        void setReceived(bool received);


        /**
         * Mutex for sending messages.
         */
        mutex sendingMutex;
        /**
         * Mutex for receiving messages.
         */
        mutex bufferMutex;
        /**
         * Mutex for confirmation.
         */
        mutex confirmMutex;
        /**
         * Mutex for confirmation.
         */
        mutex confirmReceivedMutex;
        /**
         * Confirm condition variable.
         */
        condition_variable confirmCondition;


        /**
         * Receival flag
        */
        bool confirmReceived = false;

        /**
         * Buffer for received messages.
        */
        ResponseBuffer responseBuffer;

        /**
         * Buffer for stdin messages.
        */
        InputBuffer inputBuffer;

        /**
         * Adress of the server.
        */
        struct sockaddr_in server_address;
        /**
         * Socket for communication.
        */
        int sock;

        /**
         * UDP timeout (in ms).
        */
        unsigned short UdpTimeout;
        /**
         * UDP retransmissions count.
        */
        unsigned short UdpRetransmissions;

        string ERR_ = "ERR: ";

        /**
         * Possible client statuses.
        */
        enum Status {
            START,
            AUTH,
            OPEN,
            ERR,
            END
        } Status;

        /**
         * Current status of the client.
        */        
        enum Status CurrentStatus = Status::START;

        /**
         * Message ID (Sent)
        */
        unsigned short SentID = 0;
        /**
         * Message ID (Received)
        */
        unsigned short ReceivedID = 0;

        /**
         * User Display name
        */
    	string DisplayName = "NONE";

};

#endif // TCP_UDP_H

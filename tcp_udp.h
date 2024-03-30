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
        // Tcp(const string& serverIP, unsigned short serverPort, int sock);
        TcpUdp(sockaddr_in server_address, int sock, unsigned short UdpTimeout, unsigned short UdpRetransmissions);
        void Input(int Protocol);

        void connectToServer();

        void sendMessageTCP(const string& message);
        void sendMessageUDP(const char* message, size_t length);
        void sendConfirm(short ID);
        void sendByeUDP(short ID);


        // void sendMessageUDP(const string& message);

        bool checkConfirmationUDP(short expectedID, const char* msg);

        void setSocketTimeout(int sock, int timeoutMs);

        string receiveMessageTCP();
        char* receiveMessageUDP(int *err, int *bytes);

        void closeConnection();

        // void networkCommunicationThread(InputBuffer& inputBuffer, ResponseBuffer& responseBuffer);
        void networkCommunicationThread();
        // void stdinReaderThread(InputBuffer& inputBuffer);
        void stdinReaderThread();
        void confirmThread(short expectedID, const char* message, size_t length);
        // void confirmThread();
        // void receiverThread(InputBuffer& inputBuffer, ResponseBuffer& responseBuffer);
        void receiverThread();
        void signalHandlerThread(const sigset_t& signals);

        void printMessageAsHex(const char* message, size_t length);
        void printBuffer(InputBuffer& inputBuffer);

        void setReceived(bool received);

        // static TcpUdp* instance;

        // TcpUdp() {
        //     instance = this;
        // }

        // static void signalHandler(int signum);

    private:
        mutex sendingMutex;
        mutex bufferMutex;
        mutex confirmMutex;
        condition_variable confirmCondition;

        mutex confirmReceivedMutex;

        // condition_variable networkThread;
        // mutex networkMutex;
        // bool networkThreadReady = false;

        bool confirmReceived = false;

        MemoryManager memoryManager;
        ResponseBuffer responseBuffer;
        InputBuffer inputBuffer;
        struct sockaddr_in server_address;
        // string serverIP;
        // unsigned short serverPort;
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

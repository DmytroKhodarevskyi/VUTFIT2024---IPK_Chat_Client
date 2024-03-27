#include "tcp_udp.h"
// TcpUdp::TcpUdp(const string& serverIP, unsigned short serverPort, int sock)
//     : serverIP(serverIP), serverPort(serverPort), sock(sock) {}

TcpUdp::TcpUdp(sockaddr_in server_address, int sock, unsigned short UdpTimeout, unsigned short UdpRetransmissions, enum Status CurrentStatus)
    : server_address(server_address), sock(sock), UdpTimeout(UdpTimeout), UdpRetransmissions(UdpRetransmissions), CurrentStatus(CurrentStatus) {}

void TcpUdp::connectToServer()
{
    // Connect to the server

    // cerr << "Connecting to server..." << endl;
    if (sock == -1)
    {
        cerr << ERR_ << "Could not create socket\n";
        exit(1);
    }
    if (server_address.sin_family != AF_INET)
    {
        cerr << ERR_ << "Invalid address family\n";
        close(sock);
        exit(1);
    }
    if (server_address.sin_port == 0)
    {
        cerr << ERR_ << "Invalid port\n";
        close(sock);
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        cerr << ERR_ << "Could not connect to server\n";
        close(sock);
        exit(2);
    }

    // cerr << "Connected to server\n";
}

void TcpUdp::sendMessageTCP(const string &message)
{
    // Send a message to the server
    const char *message_cstr = message.c_str();

    if (sock == -1)
    {
        cerr << ERR_ << "Socket is closed\n";
        exit(1);
    }

    if (send(sock, message_cstr, strlen(message_cstr), 0) == -1)
    {
        cerr << ERR_ << "Could not send message\n";
        close(sock);
        exit(3);
    }

    // cerr << "Message sent: " << message;
}

void TcpUdp::printMessageAsHex(const char *message, size_t length)
{
    // std::cerr << "Sending message: ";
    for (size_t i = 0; i < length; ++i)
    {
        std::cerr << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(static_cast<unsigned char>(message[i])) << " ";
    }
    std::cerr << std::endl;
}

void TcpUdp::confirmThread(short expectedID, const char *message, size_t length)
{
    std::lock_guard<std::mutex> lock(confirmMutex);

    auto Timeout = chrono::milliseconds(UdpTimeout);
    auto checkInterval = chrono::milliseconds(UdpTimeout / 10);

    // printMessageAsHex(message, length);

    InputProcess inputProcess;
    const char *confirm;
    bool success = false;

    sendingMutex.lock();
    SentID++;
    sendingMutex.unlock();

    for (int i = 0; i < UdpRetransmissions; i++)
    {

        sendingMutex.lock();
        if (-1 == sendto(sock, message, length, 0, (struct sockaddr *)&server_address, sizeof(server_address)))
        {
            cerr << ERR_ << "Could not send message\n";
            // close(sock);
            // exit(3);
        }
        sendingMutex.unlock();

        auto startTime = chrono::steady_clock::now();
        chrono::steady_clock::time_point checkTime = startTime;

        while (chrono::steady_clock::now() - startTime < Timeout) {
                
                this_thread::sleep_until(checkTime + checkInterval); // Wait for the next check interval
                checkTime = chrono::steady_clock::now();
                
                // Lock and check the buffer
                std::lock_guard<std::mutex> guard(bufferMutex);
                // if (isConfirmationInBuffer(messageId)) {
                vector<unsigned char> check_ID = inputProcess.udp_construct_confirm(expectedID);

                // cerr << "\nChecking for confirmation...\n";
                responseBuffer.printBuffer();

                const char* respond 
                    = responseBuffer
                    .getMessageFromBuffer(reinterpret_cast<const char*>(check_ID.data()), check_ID.size());
                
                if (respond != nullptr) {
                    responseBuffer
                    .removeMessageFromBuffer(reinterpret_cast<const char*>(check_ID.data()), check_ID.size());
                    success = true;
                    // cerr << "Confirmation received\n";
                    responseBuffer.printBuffer();
                    break;
                } 
                //     confirmed = true;
                //     break; // Exit the loop if confirmation is found
                // }
        }

        if (success) {
            confirmeReceived = true;
            confirmCondition.notify_one();
            break;
        }
        else {
            cerr << "ERR: Timeout occurred while waiting for a message, retrying...\n";
        }

        // sendMessageUDP(sequence, messageBytes.size());
        // int err = 0;
        // // confirm = receiveMessageUDP(&err);
        // if (err == 1) {
        //     // sendMessageUDP(sequence, messageBytes.size());
        //     continue;
        // }
        // // const char* confirm = receiveMessageUDP();
        // receivedID++;
        // if (!checkConfirmationUDP(sentID, confirm)) {
        //     // cerr << "ERR FROM " + DisplayName + ": Received confirmation ID is wrong -";
        //     // cerr.write(confirm, 2);/
        //     // cerr << "\n";

        //     // close(sock);
        //     // exit(1);
        // }

        // const char* respond = receiveMessageUDP();
        // if (respond[0] == 0x01) {

        //     break;
        // }
    }


}

void TcpUdp::sendConfirm(short ID) {
    InputProcess inputProcess;
    auto messageBytes = inputProcess.udp_construct_confirm(ID);
    auto length = messageBytes.size();

    const char *message = reinterpret_cast<const char *>(messageBytes.data());

    // sendingMutex.lock();
    if (-1 == sendto(sock, message, length, 0, (struct sockaddr *)&server_address, sizeof(server_address)))
    {
        cerr << ERR_ << "Could not send confirmation\n";
        // close(sock);
        // exit(3);
    }
    // sendingMutex.unlock();

}

void TcpUdp::sendByeUDP(short ID) {
    InputProcess inputProcess;
    auto messageBytes = inputProcess.udp_construct_bye(ID);
    auto length = messageBytes.size();

    const char *message = reinterpret_cast<const char *>(messageBytes.data());

    // sendingMutex.lock();
    if (-1 == sendto(sock, message, length, 0, (struct sockaddr *)&server_address, sizeof(server_address)))
    {
        cerr << ERR_ << "Could not send confirmation\n";
        // close(sock);
        // exit(3);
    }
    // sendingMutex.unlock();
}

void TcpUdp::sendMessageUDP(const char *message, size_t length)
{

	if (sock == -1) {
		cerr << ERR_ << "Socket is closed\n";
		exit(1);
	}

    // Dynamically allocate memory for a copy of the message
    auto messageCopy = std::make_unique<char[]>(length);
    std::memcpy(messageCopy.get(), message, length);


    std::thread confirmThread([this, messageCopy = std::move(messageCopy), length]() mutable {
         this->confirmThread(this->SentID, messageCopy.get(), length);
    });

    confirmThread.detach();

    // if (-1 == sendto(sock, message, length, 0, (struct sockaddr*)&server_address, sizeof(server_address))) {
    //     cerr << ERR_ << "Could not send message\n";
    //     close(sock);
    //     exit(3);
    // }

    // if (send(sock, message_cstr, strlen(message_cstr), 0) == -1) {
    //     cerr << ERR_ << "Could not send message\n";
    //     close(sock);
    //     exit(3);
    // }

    // cerr << "Message sent: " << message;
}

string TcpUdp::receiveMessageTCP()
{
    char buffer[1500];

    int bytesReceived = recv(sock, buffer, 1500, 0);

    if (bytesReceived == -1)
    {
        cerr << ERR_ << "Error in recv(). Quitting" << endl;
        close(sock);
        exit(4);
    }

    if (bytesReceived == 0)
    {
        cerr << ERR_ << "Client disconnected " << endl;
        close(sock);
        exit(5);
    }

    return string(buffer, bytesReceived);
}

char *TcpUdp::receiveMessageUDP(int *err, int *bytes)
{
    char *buffer = new char[1500];
    // char* buffer = memoryManager.allocate<char>(1500);

    struct sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);   
    memset(&senderAddr, 0, sizeof(senderAddr));

    // int bytesReceived = recv(sock, buffer, 1500, 0);
    // cerr << "sock: " << sock << "\n";

    int bytesReceived = recvfrom(sock, buffer, 1500, 0, (struct sockaddr *)&senderAddr, &senderAddrLen);
    // cerr << "Bytes received: " << bytesReceived << "\n";

    if (bytesReceived == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // cerr << "ERR: Timeout occurred while waiting for a message, retrying...\n";
            // memoryManager.freeSpecific(buffer);
            // close(sock);
            // exit(6);
            // memoryManager.freeSpecific(buffer);
            // cerr << "No message received yet\n";
            delete[] buffer;
            *err = 1;
            return nullptr;
        }
        else
        {
            cerr << ERR_ << "Error in recvfrom()." << endl;
            // memoryManager.freeSpecific(buffer);
            // close(sock);
            // exit(4);
            delete[] buffer;
            *err = 2;
            return nullptr;
        }
    }

    // if (bytesReceived == 0) {
    //     cerr << ERR_ << "Client disconnected " << endl;
    //     close(sock);
    //     delete[] buffer;
    //     exit(5);
    // }
    // cerr << "Bytes received: " << bytesReceived << "\n";
    *bytes = bytesReceived;
    server_address.sin_port = htons(senderAddr.sin_port);

    // return string(buffer, bytesReceived);
    return buffer;
}

void TcpUdp::closeConnection()
{
    close(sock);
}

// Function for stdin reading and buffering
// void TcpUdp::stdinReaderThread(InputBuffer& inputBuffer) {
void TcpUdp::stdinReaderThread()
{

    string line;
    while (true)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        timeval timeout;
        timeout.tv_sec = 0;       // Seconds
        timeout.tv_usec = 100000; // Microseconds

        // Check if input is available
        int ret = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
        if (ret > 0)
        {
            if (FD_ISSET(STDIN_FILENO, &readfds))
            {
                getline(cin, line);
                // cerr << "Line read: " << line << endl;
                if (line.empty())
                {
                    continue;
                }

                inputBuffer.addLine(line);
            }
        }
        else if (ret == 0)
        {
            // Timeout occurred
            if (inputBuffer.getNetwork() == false)
            {
                break;
            }
        }
        else
        {
            // Error occurred
            cerr << "ERR: Error reading from stdin" << endl;
            break;
        }
    }
}

bool TcpUdp::checkConfirmationUDP(short expectedID, const char *msg)
{
    InputProcess inputProcess;
    unsigned char *expID_bytes = inputProcess.shortToBytes(expectedID).data();

    if (msg[0] == expID_bytes[0] && msg[1] == expID_bytes[1])
    {
        return true;
    }

    return false;
}

// Placeholder function for network communication
// void TcpUdp::networkCommunicationThread(InputBuffer& inputBuffer, ResponseBuffer& responseBuffer) {
void TcpUdp::networkCommunicationThread()
{
    inputBuffer.setNetwork(true);
    string DisplayName = "NONE";
    string ServerName = "Server";
    // enum Status CurrentStatus = START;
    InputProcess inputProcess;

    InputBuffer::Protocol Protocol = inputBuffer.protocol;

    // cerr << "Protocol: " << Protocol << endl;

    while (true) { // Replace with a condition to exit the loop as needed
			if (!inputBuffer.isEmpty()) {
				auto opt_line = inputBuffer.retrieveLine();

				if (opt_line.has_value()) {
					string line = opt_line.value();

					InputProcess::Command message = {InputProcess::Command::NONE, "", "", "", "", "", 0};
					message = inputProcess.parseLine(line);

					if (message.type == InputProcess::Command::NONE ||
							message.type == InputProcess::Command::HELP)
					{
							continue;
					}

					if (message.type == InputProcess::Command::MSG) {
						if (CurrentStatus != OPEN) {
								cerr << ERR_ << "You need to authenticate first\n";
								continue;
						}

						message.displayName = DisplayName;
						string message_to_send = inputProcess.tcp_construct_msg(message);

						sendMessageTCP(message_to_send);
						cout << DisplayName + ": " + message.message + "\n";
					}

					if (message.type == InputProcess::Command::AUTH) {

						// cerr << "Authenticating...\n";

						if (CurrentStatus == START) {
								CurrentStatus = AUTH;
						} else {
								cerr << ERR_ << "You are already authenticated\n";
								continue;
						}

						DisplayName = message.displayName;

						if (Protocol == InputBuffer::Protocol::UDP) {

							// sentID++;
							auto messageBytes = inputProcess.udp_construct_auth(message, SentID);
							const char *sequence = reinterpret_cast<const char *>(messageBytes.data());

							// printMessageAsHex(sequence, messageBytes.size());

							sendMessageUDP(sequence, messageBytes.size());

							unique_lock<mutex> lock(confirmMutex);
							confirmCondition.wait(lock, [this] { return confirmReceived; });
							confirmReceived = false;

							char ReplyID = 0x01;
							const char* reply = responseBuffer.getMessageByIDFromBuffer(ReplyID);

							if (reply != nullptr) {
								const char result = reply[3];
								unsigned short msgID = (static_cast<unsigned char>(reply[1]) << 8) 
																			| static_cast<unsigned char>(reply[2]);

								// cerr << "Received ID: " << msgID << "\n";
								std::cerr << "Received ID: " << std::hex << msgID << std::dec << "\n";
								ReceivedID = msgID;

								sendConfirm(ReceivedID);

								if (result == 0x01)
								{
									CurrentStatus = OPEN;
									cerr << "Success: Auth success.\n";
										


									continue;	
										// cerr << "Server: " + DisplayName + " has joined default\n";
								}
								else if (result == 0x00)
								{
									cerr << "ERR FROM " + ServerName + ": " + "Auth is failed, concider retrying" + "\n";
									// sendMessageUDP("BYE\r\n");
									// break;
									continue;
								} 
								else {
									cerr << "ERR FROM " + ServerName + ": " + "Unknown reply" + "\n";
									continue;
								}
							}

							char ErrorID = 0xFE;
							const char* error = responseBuffer.getMessageByIDFromBuffer(ErrorID);

							if (error != nullptr) {
									cerr << "ERR FROM " + ServerName + ": " + "Server Error" + "\n";

									sendByeUDP(SentID);
									// sendMessageUDP("BYE\r\n");
									break;
							}

						}

								else if (Protocol == InputBuffer::Protocol::TCP)
								{
										string message_to_send = inputProcess.tcp_construct_auth(message);

										CurrentStatus = AUTH;

										sendMessageTCP(message_to_send);

										string respond = receiveMessageTCP();
										if (inputProcess.parseRespondAuth(respond))
										{
												CurrentStatus = OPEN;
												cerr << "Success: Auth success.\n";
												// cerr << "Server: " + DisplayName + " has joined default\n";
										}
										else
										{
												cerr << "ERR FROM " + DisplayName + ": " + respond + "\n";
												sendMessageTCP("BYE\r\n");
												break;
										}
								}
								// cerr << "Received message: " << respond << endl;
						}

						if (message.type == InputProcess::Command::JOIN)
						{

								if (CurrentStatus != OPEN)
								{
										cerr << ERR_ << "You need to authenticate first\n";
										continue;
								}

								message.displayName = DisplayName;
								string message_to_send = inputProcess.tcp_construct_join(message);
								// cerr << "Sending message: " << message_to_send << endl;

								sendMessageTCP(message_to_send);

								string respond = receiveMessageTCP();
								if (inputProcess.parseRespondOpen(respond))
								{
										CurrentStatus = OPEN;
										cerr << "Success: Join success.\n";
										// cerr << "Server: " + DisplayName + " has joined " + message.channel + "\n";
								}
								else
								{
										cerr << "ERR FROM " + DisplayName + ": " + respond + "\n";
										sendMessageTCP("BYE\r\n");
										break;
								}
						}
					}
        }
        else
        {

            if (cin.eof())
            {
                // cerr << "EOF detected, exiting network thread" << endl;
                sendMessageTCP("BYE\r\n");
                break;
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }

    cerr << "Network thread exiting" << endl;
    inputBuffer.setNetwork(false);
}

// void TcpUdp::receiverThread(InputBuffer& inputBuffer, ResponseBuffer& responseBuffer) {
void TcpUdp::receiverThread()
{
    // Example: Process server replies here
    // This could involve calling recv in a loop, handling received data, etc.
    // cerr << "Receiving message..." << endl;
    while (inputBuffer.getNetwork() == true) {   // Replace with a condition to exit the loop as needed
        // while (true) { // Replace with a condition to exit the loop as needed
        int err = 0;
        int bytes = 0;
        const char *message = receiveMessageUDP(&err, &bytes);

        if (err == 2)
        {
            cerr << "ERR: Error receiving message, exiting receiver thread" << endl;
            break;
        }

        if (message != nullptr)
        {
            cerr << "Received message: " << message << endl;
            responseBuffer.addMessageToBuffer(message, bytes);
        }

        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // responseBuffer.printBuffer();
        // Timeout occurred, retry
        // if (err == 1) {
        // continue;
        // }

        // cerr << "Received message: " << message << endl;
        // cout << message;
    }

		responseBuffer.clearBuffer();

    cerr << "Receiver thread exiting" << endl;
}

void TcpUdp::printBuffer(InputBuffer &inputBuffer)
{
    const vector<string> &buffer = inputBuffer.getBuffer();
    if (buffer.empty())
    {
        cerr << "Buffer is empty" << endl;
        return;
    }

    for (const auto &line : buffer)
    {
        cerr << line << endl;
    }
}

void TcpUdp::setSocketTimeout(int sock, int timeoutMs)
{
    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        cerr << "ERR: Error setting socket timeout.\n";
        close(sock);
        exit(1);
        // Handle error, maybe return or exit
    }
}

void TcpUdp::Input(int Protocol)
{

    // cerr << "Starting threads" << endl;
    // InputBuffer inputBuffer;
    // ResponseBuffer responseBuffer;

    if (Protocol == 1)
    {
        inputBuffer.protocol = InputBuffer::Protocol::TCP;
        cerr << "Connecting to server..." << endl;
        connectToServer();

        std::thread inputThread(&TcpUdp::stdinReaderThread, this);
        std::thread networkThread(&TcpUdp::networkCommunicationThread, this);

        // thread inputThread([this, &inputBuffer] { this->stdinReaderThread(inputBuffer); });
        // thread networkThread([this, &inputBuffer, &responseBuffer] { this->networkCommunicationThread(inputBuffer, responseBuffer); });

        inputThread.join();
        networkThread.join();
    }
    else
    {
        // setSocketTimeout(sock, UdpTimeout);
        inputBuffer.protocol = InputBuffer::Protocol::UDP;
        // cerr << "Starting threads" << endl;

        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1)
        {
            //FIXME: Handle err
            // Handle error
        }
        flags = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        if (flags == -1)
        {
            //FIXME: Handle err
            // Handle error
        }
        // cerr << "Starting threads" << endl;
        // cerr << "Starting input thread" << endl;
        // cerr << "Starting network thread" << endl;

        thread inputThread(&TcpUdp::stdinReaderThread, this);
        thread networkThread(&TcpUdp::networkCommunicationThread, this);
        thread receiverThread(&TcpUdp::receiverThread, this);

        // thread inputThread([this, &inputBuffer] { this->stdinReaderThread(inputBuffer); });
        // thread networkThread([this, &inputBuffer, &responseBuffer] { this->networkCommunicationThread(inputBuffer, responseBuffer); });
        // thread receiverThread([this, &inputBuffer, &responseBuffer] { this->receiverThread(inputBuffer, responseBuffer); });

        inputThread.join();
        networkThread.join();
        receiverThread.join();
    }

    // char* text = memoryManager.allocate<char>(1500);
    // // text = "Hello";
    // std::strcpy(text, "Hello");

    // cerr << "Sending message: " << text << endl;

    // memoryManager.freeSpecific(text);

    // cerr << "Exiting" << endl;
}
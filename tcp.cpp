#include "tcp.h"
#include "input_process.h"


// Tcp::Tcp(const string& serverIP, unsigned short serverPort, int sock) 
//     : serverIP(serverIP), serverPort(serverPort), sock(sock) {}

Tcp::Tcp(sockaddr_in server_address, int sock) 
    : server_address(server_address), sock(sock) {}

void Tcp::connectToServer() {
    // Connect to the server

    // cerr << "Connecting to server..." << endl;
    if (sock == -1) {
        cerr << ERR_ << "Could not create socket\n";
        exit(1);
    }
    if (server_address.sin_family != AF_INET) {
        cerr << ERR_ << "Invalid address family\n";
        close(sock);
        exit(1);
    }
    if (server_address.sin_port == 0) {
        cerr << ERR_ << "Invalid port\n";
        close(sock);
        exit(1);
    }

    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        cerr << ERR_ << "Could not connect to server\n";
        close(sock);
        exit(2);
    }

    // cerr << "Connected to server\n";
}

void Tcp::sendMessage(const string& message) {
    // Send a message to the server
    const char* message_cstr = message.c_str();
    if (send(sock, message_cstr, strlen(message_cstr), 0) == -1) {
        cerr << ERR_ << "Could not send message\n";
        close(sock);
        exit(3);
    }

    // cerr << "Message sent: " << message;
}

string Tcp::receiveMessage() {
    char buffer[1024];

    // cerr << "Receiving message..." << endl;
    int bytesReceived = recv(sock, buffer, 1024, 0);
    // cerr << "Received " << bytesReceived << " bytes" << endl;

    if (bytesReceived == -1) {
        cerr << ERR_ << "Error in recv(). Quitting" << endl;
        close(sock);
        exit(4);
    }

    if (bytesReceived == 0) {
        cerr << ERR_ << "Client disconnected " << endl;
        close(sock);
        exit(5);
    }

    return string(buffer, bytesReceived);
}

void Tcp::closeConnection() {
    close(sock);
}

// Function for stdin reading and buffering
void Tcp::stdinReaderThread(InputBuffer& inputBuffer) {
    // string line;

    // // cerr << "Reading lines..." << endl;

    // while(true) {

    //     if (inputBuffer.getNetwork() == false) {
    //         break;
    //     }

    //     if (inputBuffer.getNetwork() && getline(cin, line)){

    //         if (inputBuffer.getNetwork() == false) {
    //             break;
    //         }

    //         if (line.empty()) {
    //             continue;
    //         }
    //         // cerr << "Line read, adding to buffer." << endl;
    //         inputBuffer.addLine(line);
    //         // cerr << "Read line completed, buffer now: " << endl;
    //         // printBuffer(inputBuffer);
    //     } else {
    //         // cerr << "No input, sleeping..." << endl;
    //         // this_thread::sleep_for(chrono::milliseconds(100));
    //         break;
    //     }
    // }

    // cerr << "stdinReaderThread exiting" << endl;

    string line;
    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        timeval timeout;
        timeout.tv_sec = 0;  // Seconds
        timeout.tv_usec = 100000;  // Microseconds

        // Check if input is available
        int ret = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
        if (ret > 0) {
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                getline(cin, line);
                // cerr << "Line read: " << line << endl;
                if (line.empty()) {
                    continue;
                }

                inputBuffer.addLine(line);
            }
        }
        else if (ret == 0) {
            // Timeout occurred
            if (inputBuffer.getNetwork() == false) {
                break;
            }
        }
        else {
            // Error occurred
            cerr << "ERR: Error reading from stdin" << endl;
            break;
        }
    }


    // while (getline(cin, line)) {
    //     cerr << "Line read, adding to buffer." << endl;
    //     inputBuffer.addLine(line);
    //     cerr << "Read line completed, buffer now: " << endl;
    //     // printBuffer(inputBuffer);
    // }


}

// Placeholder function for network communication
void Tcp::networkCommunicationThread(InputBuffer& inputBuffer) {
    // Example: Process server replies here
    // This could involve calling recv in a loop, handling received data, etc.
    string DisplayName = "NONE";
    enum Status CurrentStatus = START;  
    InputProcess inputProcess;

    while (true) { // Replace with a condition to exit the loop as needed
        if (!inputBuffer.isEmpty()) {
            // cerr << "Buffer is not empty, processing line..." << endl;
            auto opt_line = inputBuffer.retrieveLine();
            // cerr << "Buffer: " << endl;
            // printBuffer(inputBuffer);


            if (opt_line.has_value()) {
                string line = opt_line.value();
            // cerr << "Retrieved line: " << line.value() << endl;
            // if (line) {
                // Send the line over the network
                // cout << "Sending line over network... " << endl;

                InputProcess::Command message = { InputProcess::Command::NONE, "", "", "", "", ""};
                message = inputProcess.parseLine(line);

                if (message.type == InputProcess::Command::NONE ||
                    message.type == InputProcess::Command::HELP) {
                        continue;
                }

                if (message.type == InputProcess::Command::MSG) {
                    if (CurrentStatus != OPEN) {
                        cerr << ERR_ << "You need to authenticate first\n";
                        continue;
                    }

                    message.displayName = DisplayName;
                    string message_to_send = inputProcess.tcp_construct_msg(message);
                    // cerr << "Sending message: " << message_to_send;

                    sendMessage(message_to_send);
                    cout << DisplayName + ": " + message.message + "\n";
                }

                if (message.type == InputProcess::Command::AUTH) {

                    if (CurrentStatus == START) {
                        CurrentStatus = AUTH;
                    } else {
                        cerr << ERR_ <<"You are already authenticated\n";
                        continue;
                    }

                    DisplayName = message.displayName;
                    string message_to_send = inputProcess.tcp_construct_auth(message);

                    // cerr << "Sending message: " << message_to_send;
                    // cerr << "New Display Name: " << DisplayName << endl;

                    CurrentStatus = AUTH;

                    sendMessage(message_to_send);

                    string respond = receiveMessage();
                    if (inputProcess.parseRespondAuth(respond)) {
                        CurrentStatus = OPEN;
                        cerr << "Success: Auth success.\n";
                        cerr << "Server: " + DisplayName + " has joined default\n";
                    } else {
                        cerr << "ERR FROM " + DisplayName + ": " + respond + "\n";
                        sendMessage("BYE\r\n");
                        break;                    
                    }

                    // cerr << "Received message: " << respond << endl;

                    // cerr << "segfault here\n";
                }

                if (message.type == InputProcess::Command::JOIN) {

                    if (CurrentStatus != OPEN) {
                        cerr << ERR_ <<"You need to authenticate first\n";
                        continue;
                    }

                    message.displayName = DisplayName;
                    string message_to_send = inputProcess.tcp_construct_join(message);
                    // cerr << "Sending message: " << message_to_send << endl;


                    sendMessage(message_to_send);

                    string respond = receiveMessage();
                    if (inputProcess.parseRespondOpen(respond)) {
                        CurrentStatus = OPEN;
                        cerr << "Success: Join success.\n";
                        cerr << "Server: " + DisplayName + " has joined " + message.channel + "\n";
                    } else {
                        cerr << "ERR FROM " + DisplayName + ": " + respond + "\n";
                        sendMessage("BYE\r\n");
                        break;                    
                    }
                }

                // Replace the above line with actual network sending logic
            }
        } else {
            // Optionally sleep to prevent spinning if the buffer is empty
            // cerr << "Buffer is empty, sleeping for 100ms" << endl;
            if (cin.eof()) {
                // cerr << "EOF detected, exiting network thread" << endl;
                sendMessage("BYE\r\n");
                break;
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }

    // cerr << "Network thread exiting" << endl;
    inputBuffer.setNetwork(false);
}

void Tcp::printBuffer(InputBuffer& inputBuffer) {
    const vector<string>& buffer = inputBuffer.getBuffer();
    if (buffer.empty()) {
        cerr << "Buffer is empty" << endl;
        return;
    }

    for (const auto& line : buffer) {
        cerr << line << endl;
    }
}

void Tcp::Input() {

    // cerr << "Starting threads" << endl;
    connectToServer();
    // cerr << "Starting threads" << endl;


    InputBuffer inputBuffer;
    // InputProcess inputProcess;
    // string DisplayName = "NONE";
    // enum Status CurrentStatus = START;

    // thread inputThread(stdinReaderThread, ref(inputBuffer));
    // thread networkThread(networkCommunicationThread, ref(inputBuffer));

    // cerr << "Starting threads" << endl;
    // cerr << "Starting input thread" << endl;
    thread inputThread([this, &inputBuffer] { this->stdinReaderThread(inputBuffer); });
    // cerr << "Starting network thread" << endl;
    thread networkThread([this, &inputBuffer] { this->networkCommunicationThread(inputBuffer); });

    inputThread.join();
    // cerr << "input joined" << endl;
    networkThread.join();
    // cerr << "network joined" << endl;

    
    // string message;
    // cout << "Enter a message: ";
    // getline(cin, message);
    // sendMessage(message);


    // string line;
    // while (getline(cin, line)) {

    //     // InputProcess::Command message = {0, "NONE"};
    //     // Initialization in C++98/C++03 might look like this:
    //     InputProcess::Command message = { InputProcess::Command::NONE, "", "", "", "", ""};
    //     message = inputProcess.parseLine(line);

    //     if (message.type == InputProcess::Command::NONE ||
    //         message.type == InputProcess::Command::HELP) {
    //             continue;
    //     }

    //     if (message.type == InputProcess::Command::AUTH) {
    //         DisplayName = message.displayName;
    //         string message_to_send = inputProcess.tcp_construct_auth(message);

    //         cerr << "Sending message: " << message_to_send;
    //         cerr << "New Display Name: " << DisplayName << endl;

    //         CurrentStatus = AUTH;

    //         sendMessage(message_to_send);
    //         string respond = receiveMessage();

    //         cerr << "Received message: " << respond << endl;

    //         // cerr << "segfault here\n";
    //     }

    //     if (message.type == InputProcess::Command::JOIN) {
    //         message.displayName = DisplayName;
    //         string message_to_send = inputProcess.tcp_construct_join(message);
    //         cerr << "Sending message: " << message_to_send << endl;


    //         sendMessage(message_to_send);
    //     }
    //     // cout << "Read line completed " << line << endl;
    //     // sendMessage(message);
    // }

    // cerr << "Read line completed " << line << endl;


  
}
// #include "udp.h"

// Udp::Udp(sockaddr_in server_address, int sock) 
//     : server_address(server_address), sock(sock) {}


// void Udp::sendMessage(const std::string& message) {
//     // Send a message to the server
//     const char* message_cstr = message.c_str();
//     if (send(sock, message_cstr, strlen(message_cstr), 0) == -1) {
//         std::cerr << "Could not send message\n";
//         close(sock);
//         exit(3);
//     }
// }

// void Udp::closeConnection() {
//     close(sock);
// }

// void Udp::stdinReaderThread(InputBuffer& inputBuffer) {

//     string line;
//     while (true) {
//         fd_set readfds;
//         FD_ZERO(&readfds);
//         FD_SET(STDIN_FILENO, &readfds);

//         timeval timeout;
//         timeout.tv_sec = 0;  // Seconds
//         timeout.tv_usec = 100000;  // Microseconds

//         // Check if input is available
//         int ret = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
//         if (ret > 0) {
//             if (FD_ISSET(STDIN_FILENO, &readfds)) {
//                 getline(cin, line);
//                 // cerr << "Line read: " << line << endl;
//                 if (line.empty()) {
//                     continue;
//                 }

//                 inputBuffer.addLine(line);
//             }
//         }
//         else if (ret == 0) {
//             // Timeout occurred
//             if (inputBuffer.getNetwork() == false) {
//                 break;
//             }
//         }
//         else {
//             // Error occurred
//             cerr << "ERR: Error reading from stdin" << endl;
//             break;
//         }
//     }

// }

// void Udp::networkCommunicationThread(InputBuffer& inputBuffer) {
//     // Example: Process server replies here
//     // This could involve calling recv in a loop, handling received data, etc.
//     string DisplayName = "NONE";
//     enum Status CurrentStatus = START;  
//     InputProcess inputProcess;

//     while (true) { // Replace with a condition to exit the loop as needed
//         if (!inputBuffer.isEmpty()) {
//             // cerr << "Buffer is not empty, processing line..." << endl;
//             auto opt_line = inputBuffer.retrieveLine();
//             // cerr << "Buffer: " << endl;
//             // printBuffer(inputBuffer);


//             if (opt_line.has_value()) {
//                 string line = opt_line.value();
//             // cerr << "Retrieved line: " << line.value() << endl;
//             // if (line) {
//                 // Send the line over the network
//                 // cout << "Sending line over network... " << endl;

//                 InputProcess::Command message = { InputProcess::Command::NONE, "", "", "", "", ""};
//                 message = inputProcess.parseLine(line);

//                 if (message.type == InputProcess::Command::NONE ||
//                     message.type == InputProcess::Command::HELP) {
//                         continue;
//                 }

//                 if (message.type == InputProcess::Command::MSG) {
//                     if (CurrentStatus != OPEN) {
//                         cerr << ERR_ << "You need to authenticate first\n";
//                         continue;
//                     }

//                     message.displayName = DisplayName;
//                     string message_to_send = inputProcess.tcp_construct_msg(message);
//                     // cerr << "Sending message: " << message_to_send;

//                     sendMessage(message_to_send);
//                     cout << DisplayName + ": " + message.message + "\n";
//                 }

//                 if (message.type == InputProcess::Command::AUTH) {

//                     if (CurrentStatus == START) {
//                         CurrentStatus = AUTH;
//                     } else {
//                         cerr << ERR_ <<"You are already authenticated\n";
//                         continue;
//                     }

//                     DisplayName = message.displayName;

                    
//                     string message_to_send = inputProcess.udp_construct_auth(message);

//                     // cerr << "Sending message: " << message_to_send;
//                     // cerr << "New Display Name: " << DisplayName << endl;

//                     CurrentStatus = AUTH;

//                     sendMessage(message_to_send);

//                     string respond = receiveMessage();
//                     if (inputProcess.parseRespondAuth(respond)) {
//                         CurrentStatus = OPEN;
//                         cerr << "Success: Auth success.\n";
//                         cerr << "Server: " + DisplayName + " has joined default\n";
//                     } else {
//                         cerr << "ERR FROM " + DisplayName + ": " + respond + "\n";
//                         sendMessage("BYE\r\n");
//                         break;                    
//                     }

//                     // cerr << "Received message: " << respond << endl;

//                     // cerr << "segfault here\n";
//                 }

//                 if (message.type == InputProcess::Command::JOIN) {

//                     if (CurrentStatus != OPEN) {
//                         cerr << ERR_ <<"You need to authenticate first\n";
//                         continue;
//                     }

//                     message.displayName = DisplayName;
//                     string message_to_send = inputProcess.tcp_construct_join(message);
//                     // cerr << "Sending message: " << message_to_send << endl;


//                     sendMessage(message_to_send);

//                     string respond = receiveMessage();
//                     if (inputProcess.parseRespondOpen(respond)) {
//                         CurrentStatus = OPEN;
//                         cerr << "Success: Join success.\n";
//                         cerr << "Server: " + DisplayName + " has joined " + message.channel + "\n";
//                     } else {
//                         cerr << "ERR FROM " + DisplayName + ": " + respond + "\n";
//                         sendMessage("BYE\r\n");
//                         break;                    
//                     }
//                 }

//                 // Replace the above line with actual network sending logic
//             }
//         } else {
//             // Optionally sleep to prevent spinning if the buffer is empty
//             // cerr << "Buffer is empty, sleeping for 100ms" << endl;
//             if (cin.eof()) {
//                 // cerr << "EOF detected, exiting network thread" << endl;
//                 sendMessage("BYE\r\n");
//                 break;
//             }
//             this_thread::sleep_for(chrono::milliseconds(100));
//         }
//     }

//     // cerr << "Network thread exiting" << endl;
//     inputBuffer.setNetwork(false);
// }

// void Udp::Input() {
//     // std::string message;
//     // std::cout << "Enter a message: ";
//     // std::getline(std::cin, message);
//     // sendMessage(message);
//     // std::string line;

//     // while (std::getline(std::cin, line)) {
//     //     std::cout << "Read line: " << line << std::endl;
//     // }

//     InputBuffer inputBuffer;
//     // InputProcess inputProcess;
//     // string DisplayName = "NONE";
//     // enum Status CurrentStatus = START;

//     // thread inputThread(stdinReaderThread, ref(inputBuffer));
//     // thread networkThread(networkCommunicationThread, ref(inputBuffer));

//     // cerr << "Starting threads" << endl;
//     // cerr << "Starting input thread" << endl;
//     thread inputThread([this, &inputBuffer] { this->stdinReaderThread(inputBuffer); });
//     // cerr << "Starting network thread" << endl;
//     thread networkThread([this, &inputBuffer] { this->networkCommunicationThread(inputBuffer); });

//     inputThread.join();
//     // cerr << "input joined" << endl;
//     networkThread.join();
//     // cerr << "network joined" << endl;
// }
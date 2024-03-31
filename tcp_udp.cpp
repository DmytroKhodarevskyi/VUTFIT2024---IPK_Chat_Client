#include "tcp_udp.h"
// TcpUdp::TcpUdp(const string& serverIP, unsigned short serverPort, int sock)
//     : serverIP(serverIP), serverPort(serverPort), sock(sock) {}

TcpUdp::TcpUdp(sockaddr_in server_address, int sock, unsigned short UdpTimeout, unsigned short UdpRetransmissions)
		: server_address(server_address), sock(sock), UdpTimeout(UdpTimeout), UdpRetransmissions(UdpRetransmissions) {}

void TcpUdp::connectToServer()
{
	// Connect to the server

	// cerr << "Connecting to server..." << endl;
	if (sock == -1)
	{
		cerr << ERR_ << "Could not create socket" << endl;
		exit(1);
	}
	if (server_address.sin_family != AF_INET)
	{
		cerr << ERR_ << "Invalid address family" << endl;
		close(sock);
		exit(1);
	}
	if (server_address.sin_port == 0)
	{
		cerr << ERR_ << "Invalid port" << endl;
		close(sock);
		exit(1);
	}

	if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
	{
		cerr << ERR_ << "Could not connect to server" << endl;
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
		cerr << ERR_ << "Socket is closed" << endl;
		exit(1);
	}

	if (send(sock, message_cstr, strlen(message_cstr), 0) == -1)
	{
		cerr << ERR_ << "Could not send message" << endl;
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
	// const char *confirm;
	bool success = false;

	sendingMutex.lock();
	SentID++;
	sendingMutex.unlock();

	for (int i = 0; i < UdpRetransmissions; i++)
	{

		sendingMutex.lock();
		if (-1 == sendto(sock, message, length, 0, (struct sockaddr *)&server_address, sizeof(server_address)))
		{
			cerr << ERR_ << "Could not send message" << endl;
			// close(sock);
			// exit(3);
		}
		sendingMutex.unlock();

		auto startTime = chrono::steady_clock::now();
		chrono::steady_clock::time_point checkTime = startTime;

		while (chrono::steady_clock::now() - startTime < Timeout)
		{

			this_thread::sleep_until(checkTime + checkInterval); // Wait for the next check interval
			checkTime = chrono::steady_clock::now();

			// Lock and check the buffer
			std::lock_guard<std::mutex> guard(bufferMutex);
			// if (isConfirmationInBuffer(messageId)) {
			vector<unsigned char> check_ID = inputProcess.udp_construct_confirm(expectedID);

			// cerr << "\nChecking for confirmation...\n";
			// cerr << endl;
			// responseBuffer.printBuffer();

			const char *response = responseBuffer
																 .getMessageFromBuffer(reinterpret_cast<const char *>(check_ID.data()), check_ID.size());

			if (response != nullptr)
			{
				responseBuffer
						.removeMessageFromBuffer(reinterpret_cast<const char *>(check_ID.data()), check_ID.size());
				success = true;
				// cerr << "Confirmation received for ID: " << expectedID << "\n";
				// cerr << "Confirmation received\n";
				// responseBuffer.printBuffer();
				break;
				// return;
			}
			//     confirmed = true;
			//     break; // Exit the loop if confirmation is found
			// }
		}

		if (success)
		{
			// confirmReceived = true;
			setReceived(true);
			confirmCondition.notify_one();
			// break;
			return;
		}
		else
		{
			cerr << "ERR: Timeout occurred while waiting for a message, retrying..." << endl;
		}
	}

	// std::this_thread::sleep_for(std::chrono::milliseconds(100));
	if (!success)
	{

		cerr << "ERR: Packet lost, retransmission limit reached" << endl;
		sendByeUDP(SentID);

		// confirmReceived = true;
		setReceived(true);
		confirmCondition.notify_one();
		inputBuffer.setNetwork(false);
	}
}

void TcpUdp::sendConfirm(short ID)
{
	InputProcess inputProcess;
	auto messageBytes = inputProcess.udp_construct_confirm(ID);
	auto length = messageBytes.size();

	const char *message = reinterpret_cast<const char *>(messageBytes.data());

	// sendingMutex.lock();
	if (-1 == sendto(sock, message, length, 0, (struct sockaddr *)&server_address, sizeof(server_address)))
	{
		cerr << ERR_ << "Could not send confirmation" << endl;
		// close(sock);
		// exit(3);
	}
	// sendingMutex.unlock();

	// cerr << "Confirmation sent, ID:" << ID << endl;

	ReceivedID++;
}

void TcpUdp::sendByeUDP(short ID)
{
	InputProcess inputProcess;
	auto messageBytes = inputProcess.udp_construct_bye(ID);
	auto length = messageBytes.size();

	const char *message = reinterpret_cast<const char *>(messageBytes.data());

	sendingMutex.lock();
	if (-1 == sendto(sock, message, length, 0, (struct sockaddr *)&server_address, sizeof(server_address)))
	{
		cerr << ERR_ << "Could not send bye" << endl;
		// close(sock);
		// exit(3);
	}
	sendingMutex.unlock();

	// sendMessageUDP(message, length);
}

void TcpUdp::sendMessageUDP(const char *message, size_t length)
{

	if (sock == -1)
	{
		cerr << ERR_ << "Socket is closed" << endl;
		exit(1);
	}

	// Dynamically allocate memory for a copy of the message
	auto messageCopy = std::make_unique<char[]>(length);
	std::memcpy(messageCopy.get(), message, length);

	std::thread confirmThread([this, messageCopy = std::move(messageCopy), length]() mutable
														{ this->confirmThread(this->SentID, messageCopy.get(), length); });

	confirmThread.detach();
	// cerr << "why is it looping?\n";

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

	// cerr << "new port: " << senderAddr.sin_port << endl;
	// server_address.sin_port = htons(senderAddr.sin_port);
	// server_address.sin_port = ntohs(senderAddr.sin_port);
	server_address.sin_port = senderAddr.sin_port;

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
	// while (true)
	while (inputBuffer.getNetwork() == true)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);

		timeval timeout;
		timeout.tv_sec = 0;				// Seconds
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

	// cerr << "Stdin reader thread exiting" << endl;
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
	string ServerName = "Server";
	// enum Status CurrentStatus = START;
	InputProcess inputProcess;

	SentID = 0;

	InputBuffer::Protocol Protocol = inputBuffer.protocol;

	// cerr << "Protocol: " << Protocol << endl;

	// while (true)
	while (inputBuffer.getNetwork() == true)
	{ // Replace with a condition to exit the loop as needed
		if (!inputBuffer.isEmpty())
		{
			auto opt_line = inputBuffer.retrieveLine();

			if (opt_line.has_value())
			{
				string line = opt_line.value();

				InputProcess::Command message = {InputProcess::Command::NONE, "", "", "", "", "", 0};
				message = inputProcess.parseLine(line);

				if (message.type == InputProcess::Command::RENAME)
					DisplayName = message.displayName;

				if (message.type == InputProcess::Command::NONE ||
						message.type == InputProcess::Command::HELP)
				{
					continue;
				}

				if (message.type == InputProcess::Command::MSG)
				{
					if (CurrentStatus != OPEN)
					{
						cerr << ERR_ << "You need to authenticate first" << endl;
						continue;
					}

					message.displayName = DisplayName;

					if (Protocol == InputBuffer::Protocol::UDP)
					{

						auto messageBytes = inputProcess.udp_construct_msg(message, SentID);
						const char *sequence = reinterpret_cast<const char *>(messageBytes.data());
						size_t length = messageBytes.size();

						sendMessageUDP(sequence, length);
						unique_lock<mutex> lock(confirmMutex);
						confirmCondition.wait(lock, [this]
																	{ return confirmReceived; });
						// confirmReceived = false;
						setReceived(false);

						// cout << "HELLO?\n";
						// cerr << "HELLO?\n";
						cout << DisplayName << ": " << message.message << endl;

						short msgID = 0x04;
						short errID = 0xFE;
						short byeID = 0xFF;

						// const char *serv_msg = nullptr;
						// size_t msg_length = 0;

						const char *err = nullptr;
						size_t err_length = 0;

						const char *bye = nullptr;
						size_t bye_length = 0;

						// while (true) {
						// tie(serv_msg, msg_length) = responseBuffer.getMessageByIDFromBuffer(msgID);
						// if (serv_msg != nullptr)
						// {
						// 	unsigned short msgID = (static_cast<unsigned char>(serv_msg[1]) << 8) | static_cast<unsigned char>(serv_msg[2]);

						// 	sendConfirm(msgID);

						// 	const char *start = &serv_msg[3];						// Start from the 3rd byte
						// 	const char *end = std::strchr(start, '\0'); // Find the next zero byte

						// 	if (end == nullptr)
						// 	{
						// 		cerr << "ERR: Server name is not found" << endl;
						// 		sendByeUDP(SentID);
						// 		responseBuffer.removeMessageFromBuffer(serv_msg, msg_length);
						// 		// inputBuffer.setNetwork(false);
						// 		break;
						// 	}

						// 	ServerName = string(start, end - start);
						// 	const char *nextStart = end + 1;
						// 	const char *nextEnd = std::strchr(nextStart, '\0');

						// 	string reply_msg = string(nextStart, nextEnd - nextStart);
						// 	// }

						// 	cout << ServerName + ": " << reply_msg << endl;

						// 	responseBuffer.removeMessageFromBuffer(serv_msg, msg_length);
						// }

						tie(err, err_length) = responseBuffer.getMessageByIDFromBuffer(errID);
						if (err != nullptr)
						{
							unsigned short msgID = (static_cast<unsigned char>(err[1]) << 8) | static_cast<unsigned char>(err[2]);

							sendConfirm(msgID);
							// cerr << "ERR FROM " + ServerName + ": " + "Server Error" + "\n";
							const char *start = &err[3];								// Start from the 3rd byte
							const char *end = std::strchr(start, '\0'); // Find the next zero byte

							if (end == nullptr)
							{
								cerr << "ERR: Server name is not found" << endl;
								sendByeUDP(SentID);
								responseBuffer.removeMessageFromBuffer(err, err_length);
								// inputBuffer.setNetwork(false);
								break;
							}

							ServerName = string(start, end - start);
							const char *nextStart = end + 1;
							const char *nextEnd = std::strchr(nextStart, '\0');

							string reply_msg = string(nextStart, nextEnd - nextStart);
							// }

							// if (rep_result == 0x01 && msg_from_server != nullptr)
							// {
							cerr << "ERR FROM " << ServerName << ": " << reply_msg << endl;

							sendByeUDP(SentID);
							responseBuffer.removeMessageFromBuffer(err, err_length);
							// inputBuffer.setNetwork(false);
							break;
						}

						tie(bye, bye_length) = responseBuffer.getMessageByIDFromBuffer(byeID);
						if (bye != nullptr)
						{
							unsigned short msgID = (static_cast<unsigned char>(bye[1]) << 8) | static_cast<unsigned char>(bye[2]);

							sendConfirm(msgID);

							sendByeUDP(SentID);
							// cerr << "ERR FROM " + ServerName + ": " + "Server Error" + "\n";
							responseBuffer.removeMessageFromBuffer(bye, bye_length);
							break;
						}

						// std::this_thread::sleep_for(std::chrono::milliseconds(100));
						// }

						// if (error) {
						// break;
						// }

						continue;
					}
					else if (Protocol == InputBuffer::Protocol::TCP)
					{
						string message_to_send = inputProcess.tcp_construct_msg(message);

						sendMessageTCP(message_to_send);
						cout << DisplayName << ": " << message.message << endl;
					}
				}

				if (message.type == InputProcess::Command::AUTH)
				{

					// cerr << "Authenticating...\n";

					if (CurrentStatus == START)
					{
						CurrentStatus = AUTH;
					}
					else
					{
						cerr << ERR_ << "You are already authenticated" << endl;
						continue;
					}

					DisplayName = message.displayName;

					if (Protocol == InputBuffer::Protocol::UDP)
					{

						// sentID++;
						auto messageBytes = inputProcess.udp_construct_auth(message, SentID);
						const char *sequence = reinterpret_cast<const char *>(messageBytes.data());

						// printMessageAsHex(sequence, messageBytes.size());

						sendMessageUDP(sequence, messageBytes.size());
						// cerr << "IS OT ENDLESS?\n";

						// cerr << "LOCKED\n";
						unique_lock<mutex> lock(confirmMutex);
						confirmCondition.wait(lock, [this]
																	{ return confirmReceived; });
						// confirmReceived = false;
						setReceived(false);
						// cerr << "UNLOCKED\n";

						char ReplyID = 0x01;
						char ErrorID = 0xFE;
						const char *reply = nullptr;
						size_t rep_length = 0;
						// const char* reply = responseBuffer.getMessageByIDFromBuffer(ReplyID);
						const char *error = nullptr;
						size_t error_length = 0;
						// cerr << "Reply: ";
						// cerr << "====================================================\n";
						// printMessageAsHex(reply, rep_length);
						// cerr << "====================================================\n";
						// cerr << "\n";
						// bool reply_found = false;
						int what_to_do = 0; // 1 - continue, 2 - break

						// Define your timeout duration in milliseconds
						chrono::milliseconds timeout(UdpTimeout); // For 5 seconds, for example
						chrono::steady_clock::time_point start = chrono::steady_clock::now();

						// Capture the starting time
						while (what_to_do == 0)
						{
							tie(reply, rep_length) = responseBuffer.getMessageByIDFromBuffer(ReplyID);
							// tie(error, error_length) = responseBuffer.getMessageByIDFromBuffer(ErrorID);

							if (reply != nullptr)
							{

								const char result = reply[3];
								unsigned short msgID = (static_cast<unsigned char>(reply[1]) << 8) | static_cast<unsigned char>(reply[2]);

								// cerr << "Received ID: " << msgID << "\n";
								// std::cerr << "Received ID: " << std::hex << msgID << std::dec << "\n";
								ReceivedID = msgID;
								// cerr << "Result: " << std::hex << result << std::dec << "\n";
								// cerr << "Result: " << std::hex << static_cast<unsigned>(result) << std::dec << "\n";
								// cerr << "Result: " << std::hex << static_cast<unsigned>(result) << std::dec << "\n";

								sendConfirm(ReceivedID);
								// responseBuffer.removeMessageFromBuffer(reply, rep_length);

								if (result == 0x01)
								{
									CurrentStatus = OPEN;

									const char *start = &reply[6];							// Start from the 3rd byte
									const char *end = std::strchr(start, '\0'); // Find the next zero byte

									// cerr << "Whole reply:";
									// printMessageAsHex(reply, rep_length);
									// cerr << "----------------------------------\n";

									// cerr << "Reply: ";
									// printMessageAsHex(start, end - start);
									// cerr << "----------------------------------\n";

									if (end == nullptr)
									{
										cerr << "ERR: undefined message in reply" << endl;
										what_to_do = 2; // continue
																		// continue;
																		break;
									}

									string reply_msg = string(start, end - start);

									cerr << "Success: " << reply_msg << endl;

									responseBuffer.removeMessageFromBuffer(reply, rep_length);

									// const char *msg_from_server = nullptr;
									// size_t msg_length = 0;

									// std::this_thread::sleep_for(std::chrono::milliseconds(300));

									// char MsgID = 0x04;
									// // const char* msg_from_server = responseBuffer.getMessageByIDFromBuffer(MsgID);
									// // responseBuffer.printBuffer();
									// tie(msg_from_server, msg_length) = responseBuffer.getMessageByIDFromBuffer(MsgID);
									// // responseBuffer.printBuffer();

									// if (msg_from_server != nullptr)
									// {

									// 	// cerr << "Message from server: ";
									// 	// printMessageAsHex(msg_from_server, msg_length);
									// 	char msg_ID = (static_cast<unsigned char>(msg_from_server[1]) << 8) | static_cast<unsigned char>(msg_from_server[2]);
									// 	ReceivedID = msg_ID;
									// 	sendConfirm(ReceivedID);

									// 	const char *start = &msg_from_server[3];		// Start from the 3rd byte
									// 	const char *end = std::strchr(start, '\0'); // Find the next zero byte

									// 	if (end == nullptr)
									// 	{
									// 		cerr << "ERR: Server name is not found" << endl;
									// 		what_to_do = 1; // continue
									// 										// continue;
									// 	}

									// 	ServerName = string(start, end - start);
									// 	const char *nextStart = end + 1;
									// 	const char *nextEnd = std::strchr(nextStart, '\0');

									// 	string reply_msg = string(nextStart, nextEnd - nextStart);

									// 	// cerr << "SERVER MESSAGE : " << reply_msg << endl;
									// 	cout << ServerName << ": " << reply_msg << endl;

									// 	// sendConfirm(ReceivedID);
									// 	responseBuffer.removeMessageFromBuffer(msg_from_server, msg_length);
									// 	// cerr << "REPLY REMOVED:";
									// 	// printMessageAsHex(reply, rep_length);
									// 	responseBuffer.removeMessageFromBuffer(reply, rep_length);

									// 	// responseBuffer.printBuffer();
									// 	what_to_do = 1; // continue
									// 									// continue;
									// }

									// what_to_do = 1; // continue
									// responseBuffer.removeMessageFromBuffer(reply, rep_length);

									// continue;
									// cerr << "Server: " + DisplayName + " has joined default\n";

									what_to_do = 1; // continue
								}
								else if (result == 0x00)
								{
									// cerr << "ERR FROM " + ServerName + ": " + "Auth is failed, concider retrying" + "\n";

									const char *start = &reply[6];							// Start from the 3rd byte
									const char *end = std::strchr(start, '\0'); // Find the next zero byte

									if (end == nullptr)
									{
										cerr << "ERR: Undefined message in reply" << endl;
										what_to_do = 2; // continue
																		// continue;
											// continue;
											break;
									}

									string reply_msg = string(start, end - start);

									cerr << "Failure: " << reply_msg << endl;

									CurrentStatus = START;
									responseBuffer.removeMessageFromBuffer(reply, rep_length);

									// sendMessageUDP("BYE\r\n");
									// break;
									what_to_do = 1; // continue
																	// continue;
								}
								else
								{
									cerr << "ERR FROM " << ServerName << ": "
											 << "Unknown reply" << endl;
									CurrentStatus = START;
									responseBuffer.removeMessageFromBuffer(reply, rep_length);
									what_to_do = 1; // continue
																	// continue;
								}
							}

							// char ErrorID = 0xFE;
							// const char* error = responseBuffer.getMessageByIDFromBuffer(ErrorID);
							// auto [error, err_length] = responseBuffer.getMessageByIDFromBuffer(ErrorID);
							// responseBuffer.printBuffer();
							tie(error, error_length) = responseBuffer.getMessageByIDFromBuffer(ErrorID);

							if (error != nullptr)
							{
								unsigned short msgID = (static_cast<unsigned char>(error[1]) << 8) | static_cast<unsigned char>(error[2]);

								sendConfirm(msgID);
								// cerr << "ERR FROM " + ServerName + ": " + "Server Error" + "\n";
								const char *start = &error[3];							// Start from the 3rd byte
								const char *end = std::strchr(start, '\0'); // Find the next zero byte

								cerr << "ERR hex: ";
								printMessageAsHex(error, error_length);

								if (end == nullptr)
								{
									cerr << "ERR: Server name is not found" << endl;
									sendByeUDP(SentID);
									// responseBuffer.removeMessageFromBuffer(error, error_length);
									// inputBuffer.setNetwork(false);
									what_to_do = 2; // break
																	// break;
								}

								ServerName = string(start, end - start);
								const char *nextStart = end + 1;
								const char *nextEnd = std::strchr(nextStart, '\0');

								string err_msg = string(nextStart, nextEnd - nextStart);

								cerr << "ERR FROM " << ServerName << ": " << err_msg << endl;

								sendByeUDP(SentID);
								// sendMessageUDP("BYE\r\n");
								responseBuffer.removeMessageFromBuffer(error, error_length);
								what_to_do = 2; // break
																// break;
							}

							// responseBuffer.printBuffer();
							if (what_to_do == 1 || what_to_do == 2)
								break;

							std::this_thread::sleep_for(std::chrono::milliseconds(UdpTimeout / 10));

							// responseBuffer.printBuffer();
							

							if (chrono::steady_clock::now() - start > timeout)
							{
								cerr << "ERR: Timeout of reply reached" << endl;
								sendByeUDP(SentID);
								what_to_do = 2; // break
																// break;
							}
						}


						std::this_thread::sleep_for(std::chrono::milliseconds(300));
						// responseBuffer.printBuffer();

						tie(error, error_length) = responseBuffer.getMessageByIDFromBuffer(ErrorID);

						if (error != nullptr)
						{
							unsigned short msgID = (static_cast<unsigned char>(error[1]) << 8) | static_cast<unsigned char>(error[2]);

							sendConfirm(msgID);
							// cerr << "ERR FROM " + ServerName + ": " + "Server Error" + "\n";
							const char *start = &error[3];							// Start from the 3rd byte
							const char *end = std::strchr(start, '\0'); // Find the next zero byte

							// cerr << "ERR hex: ";
							// printMessageAsHex(error, error_length);

							if (end == nullptr)
							{
								cerr << "ERR: Server name is not found" << endl;
								sendByeUDP(SentID);
								// responseBuffer.removeMessageFromBuffer(error, error_length);
								// inputBuffer.setNetwork(false);
								what_to_do = 2; // break
																// break;
							}

							ServerName = string(start, end - start);
							const char *nextStart = end + 1;
							const char *nextEnd = std::strchr(nextStart, '\0');

							string err_msg = string(nextStart, nextEnd - nextStart);

							cerr << "ERR FROM " << ServerName << ": " << err_msg << endl;

							sendByeUDP(SentID);
							// sendMessageUDP("BYE\r\n");
							responseBuffer.removeMessageFromBuffer(error, error_length);
							what_to_do = 2; // break
															// break;
						}



						// sendByeUDP(SentID);
						// cerr << "ERR: Timeout occurred while waiting for a reply.\n";
						// responseBuffer.printBuffer();

						if (what_to_do == 2)
							break;
						else if (what_to_do == 1)
							continue;
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
							cerr << "Success: Auth success." << endl;
							// cerr << "Server: " + DisplayName + " has joined default\n";
						}
						else
						{
							cerr << "ERR FROM " << DisplayName << ": " << respond << endl;
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
						cerr << ERR_ << "You need to authenticate first" << endl;
						continue;
					}

					message.displayName = DisplayName;
					// cerr << "CURRENT DISPLAY NAME: " << DisplayName << endl;

					if (Protocol == InputBuffer::Protocol::UDP)
					{

						auto messageBytes = inputProcess.udp_construct_join(message, SentID);
						const char *sequence = reinterpret_cast<const char *>(messageBytes.data());
						size_t length = messageBytes.size();

						sendMessageUDP(sequence, length);

						unique_lock<mutex> lock(confirmMutex);
						confirmCondition.wait(lock, [this]
																	{ return confirmReceived; });
						// confirmReceived = false;
						setReceived(false);

						char ReplyID = 0x01;
						char MsgID = 0x04;

						const char *reply = nullptr;
						// const char *msg_from_server = nullptr;
						size_t reply_length = 0;
						// size_t msg_length = 0;
						bool reply_found = false;
						unsigned short msgID = 0;

						const char *err = nullptr;
						size_t err_length = 0;

						const char *bye = nullptr;
						size_t bye_length = 0;

						char rep_result = 0x00;

						bool is_break = false;

						responseBuffer.printBuffer();
						// while (!(reply != nullptr && msg_from_server != nullptr))
						while (reply == nullptr)
						{
							// cerr << "SPINNING" << endl;
							if (reply == nullptr)
							{
								tie(reply, reply_length) = responseBuffer.getMessageByIDFromBuffer(ReplyID);
								// if (reply != nullptr)
								// cerr << "REPLY TIED" << endl;
								// cerr << "HEllwwwwwo?" << endl;
								// if (!reply_found)
								if (reply != nullptr)
								{
									// cerr << "HEllo?" << endl;
									rep_result = reply[3];
									if (rep_result == 0x00)
									{
										// is_break = true;
										// cerr << "Replydawdawdawd: ";
										break;
									}
									else
									{
										// cerr << "Replyaiaiaii: ";
										printMessageAsHex(reply, reply_length);
										// tie(msg_from_server, msg_length) = responseBuffer.getMessageByIDFromBuffer(MsgID);
										rep_result = reply[3];
										msgID = (static_cast<unsigned char>(reply[1]) << 8) | static_cast<unsigned char>(reply[2]);
										ReceivedID = msgID;
										sendConfirm(ReceivedID);
										reply_found = true;
									}
								}
								
							}
							// else
							// {
							// 	if (!reply_found)
							// 	{
							// 		cerr << "HEllo?" << endl;
							// 		rep_result = reply[3];
							// 		if (rep_result == 0x00)
							// 		{
							// 			// is_break = true;
							// 			cerr << "Replydawdawdawd: ";
							// 			break;
							// 		}
							// 		else
							// 		{
							// 			cerr << "Replyaiaiaii: ";
							// 			printMessageAsHex(reply, reply_length);
							// 			// tie(msg_from_server, msg_length) = responseBuffer.getMessageByIDFromBuffer(MsgID);
							// 			rep_result = reply[3];
							// 			msgID = (static_cast<unsigned char>(reply[1]) << 8) | static_cast<unsigned char>(reply[2]);
							// 			ReceivedID = msgID;
							// 			sendConfirm(ReceivedID);
							// 			reply_found = true;
							// 		}
							// 	}
							// }

							// if (msg_from_server == nullptr)
							// {
							// 	tie(msg_from_server, msg_length) = responseBuffer.getMessageByIDFromBuffer(MsgID);
							// 	// if (msg_from_server != nullptr)
							// 	// cerr << "MSG TIED" << endl;
							// }

							if (err == nullptr)
							{
								tie(err, err_length) = responseBuffer.getMessageByIDFromBuffer(0xFE);
							}
							else
							{
								unsigned short msgID = (static_cast<unsigned char>(err[1]) << 8) | static_cast<unsigned char>(err[2]);
								sendConfirm(msgID);
								// cerr << "ERR FROM " + ServerName + ": " + "Server Error" + "\n";
								const char *start = &err[3];								// Start from the 3rd byte
								const char *end = std::strchr(start, '\0'); // Find the next zero byte

								if (end == nullptr)
								{
									cerr << "ERR: Server name is not found" << endl;
									is_break = true;
									sendByeUDP(SentID);
									responseBuffer.removeMessageFromBuffer(err, err_length);
									// inputBuffer.setNetwork(false);
									// break;
									is_break = true;
								}

								ServerName = string(start, end - start);
								const char *nextStart = end + 1;
								const char *nextEnd = std::strchr(nextStart, '\0');

								string err_msg = string(nextStart, nextEnd - nextStart);
								// }

								// if (rep_result == 0x01 && msg_from_server != nullptr)
								// {
								cerr << "ERR FROM " << ServerName << ": " << err_msg << endl;

								sendByeUDP(SentID);
								responseBuffer.removeMessageFromBuffer(err, err_length);
								is_break = true;
								// inputBuffer.setNetwork(false);
								break;
							}

							if (bye == nullptr)
							{
								tie(bye, bye_length) = responseBuffer.getMessageByIDFromBuffer(0xFF);
							}
							else
							{
								unsigned short msgID = (static_cast<unsigned char>(bye[1]) << 8) | static_cast<unsigned char>(bye[2]);
								sendConfirm(msgID);

								sendByeUDP(SentID);
								// cerr << "ERR FROM " + ServerName + ": " + "Server Error" + "\n";
								responseBuffer.removeMessageFromBuffer(bye, bye_length);
								is_break = true;
								break;
							}

							// std::this_thread::sleep_for(std::chrono::milliseconds(100));

							// cerr << "Reply: ";
							// if (msg_from_server == nullptr)
							// {
							// 	cerr << "ERR: Timeout of reply reached" << endl;
							// }
						}

						cerr << "Reply: ";
						printMessageAsHex(reply, reply_length);

						if (is_break)
							break;

						// const char rep_result = reply[3];

						cerr << "Reply result: " << std::hex << static_cast<unsigned>(rep_result) << std::dec << "\n";

						// if (rep_result == 0x01 && msg_from_server != nullptr)
						if (rep_result == 0x01)
						{
							// msgID = (static_cast<unsigned char>(msg_from_server[1]) << 8) | static_cast<unsigned char>(msg_from_server[2]);
							// ReceivedID = msgID;
							// sendConfirm(ReceivedID);

							// const char *start = &msg_from_server[3];		// Start from the 3rd byte
							// const char *end = std::strchr(start, '\0'); // Find the next zero byte

							// if (end == nullptr)
							// {
							// 	cerr << "ERR: Server name is not found" << endl;
							// 	responseBuffer.removeMessageFromBuffer(msg_from_server, msg_length);
							// 	responseBuffer.removeMessageFromBuffer(reply, reply_length);

							// 	continue;
							// }

							// ServerName = string(start, end - start);
							// const char *nextStart = end + 1;
							// const char *nextEnd = std::strchr(nextStart, '\0');

							// string msg = string(nextStart, nextEnd - nextStart);
							// // }

							const char *rep_start = &reply[6];									// Start from the 3rd byte
							const char *rep_end = std::strchr(rep_start, '\0'); // Find the next zero byte

							string reply_msg = string(rep_start, rep_end - rep_start);

							// if (rep_result == 0x01 && msg_from_server != nullptr)
							// {
							cerr << "Success: " << reply_msg << endl;
							// cout << ServerName << ": " << msg << endl;
							// responseBuffer.removeMessageFromBuffer(msg_from_server, msg_length);
							responseBuffer.removeMessageFromBuffer(reply, reply_length);

							continue;
							// cerr << "Server: " + DisplayName + " has joined default\n";
						}
						else if (rep_result == 0x00)
						{
							// cerr << "ERR FROM " + ServerName + ": " + "Join is failed, concider retrying" + "\n";
							// sendMessageUDP("BYE\r\n");
							// break;
							const char *rep_start = &reply[6];									// Start from the 3rd byte
							const char *rep_end = std::strchr(rep_start, '\0'); // Find the next zero byte

							string reply_msg = string(rep_start, rep_end - rep_start);

							cerr << "Failure: " << reply_msg << endl;
							// responseBuffer.removeMessageFromBuffer(msg_from_server, msg_length);
							responseBuffer.removeMessageFromBuffer(reply, reply_length);

							continue;
						}
						else
						{
							cerr << "ERR FROM " << ServerName << ": "
									 << "Unknown reply" << endl;
							sendByeUDP(SentID);
							// responseBuffer.removeMessageFromBuffer(msg_from_server, msg_length);
							responseBuffer.removeMessageFromBuffer(reply, reply_length);
							// continue;
							break;
						}
					}
					else if (Protocol == InputBuffer::Protocol::TCP)
					{

						string message_to_send = inputProcess.tcp_construct_join(message);
						// cerr << "Sending message: " << message_to_send << endl;

						sendMessageTCP(message_to_send);

						string respond = receiveMessageTCP();
						if (inputProcess.parseRespondOpen(respond))
						{
							CurrentStatus = OPEN;
							cerr << "Success: Join success." << endl;
							// cerr << "Server: " + DisplayName + " has joined " + message.channel + "\n";
						}
						else
						{
							cerr << "ERR FROM " << DisplayName << ": " << respond << endl;
							sendMessageTCP("BYE\r\n");
							break;
						}
					}
				}
			}
			else
			{
			}
		}
		// cerr << "im waiting...\n";
		if (cin.eof())
		{
			// cerr << "EOF detected, exiting network thread" << endl;
			// sendMessageTCP("BYE\r\n");
			if (Protocol == InputBuffer::Protocol::UDP)
			{
				sendByeUDP(SentID);
			}
			else
			{
				sendMessageTCP("BYE\r\n");
			}
			break;
		}
		this_thread::sleep_for(chrono::milliseconds(100));
	}

	// cerr << "Network thread exiting" << endl;
	inputBuffer.setNetwork(false);
	// networkThreadReady = true;
	// networkThread.notify_one();
}

void TcpUdp::setReceived(bool received)
{
	lock_guard<mutex> lock(confirmReceivedMutex);
	confirmReceived = received;
	// confirmCondition.notify_one();
}

// void TcpUdp::receiverThread(InputBuffer& inputBuffer, ResponseBuffer& responseBuffer) {
void TcpUdp::receiverThread()
{
	// Example: Process server replies here
	// This could involve calling recv in a loop, handling received data, etc.
	// cerr << "Receiving message..." << endl;
	while (inputBuffer.getNetwork() == true)
	{ // Replace with a condition to exit the loop as needed
		// while (true) { // Replace with a condition to exit the loop as needed

		// cerr << "network: " << inputBuffer.getNetwork() << "\n";

		int err = 0;
		int bytes = 0;
		const char *message = receiveMessageUDP(&err, &bytes);

		if (message != nullptr)
		{
			const unsigned char firstByte = message[0];
			if (firstByte != 0xFF && firstByte != 0xFE && firstByte != 0x00 && firstByte != 0x01 &&
					firstByte != 0x02 && firstByte != 0x03 && firstByte != 0x04)
			{

				InputProcess inputProcess;
				auto messageBytes = inputProcess.udp_construct_err(SentID, DisplayName);

				const char *sequence = reinterpret_cast<const char *>(messageBytes.data());
				sendMessageUDP(sequence, messageBytes.size());

				cerr << "ERR: Invalid message received, exiting receiver thread" << endl;

				unique_lock<mutex> lock(confirmMutex);
				confirmCondition.wait(lock, [this]
															{ return confirmReceived; });
				// confirmReceived = false;
				setReceived(false);

				// cerr << "ERR: Invalid message received, exiting receiver thread" << endl;
				sendByeUDP(SentID);
				inputBuffer.setNetwork(false);
				break;
			}
		}

		if (message != nullptr)
		{
			const unsigned char firstByte = message[0];
			if (firstByte == 0x04)
			{
				// cerr << "Received BYE message, exiting receiver thread" << endl;
				short MsgID = message[1] << 8 | message[2];
				sendConfirm(MsgID);

				const char *start = &message[3];						// Start from the 3rd byte
				const char *end = std::strchr(start, '\0'); // Find the next zero byte

				if (end == nullptr)
				{
					cerr << "ERR: Server name is not found" << endl;
					sendByeUDP(SentID);
					// inputBuffer.setNetwork(false);
					break;
				}

				string ServerName = string(start, end - start);
				const char *nextStart = end + 1;
				const char *nextEnd = std::strchr(nextStart, '\0');

				string reply_msg = string(nextStart, nextEnd - nextStart);

				cout << ServerName << ": " << reply_msg << endl;
				// break;
				continue;
			}
		}

		if (message != nullptr) {
			const unsigned char firstByte = message[0];

			if (firstByte == 0xFE) {
				// cerr << "Received error message!!!" << endl;
			}
		}
			


		if (err == 2)
		{
			cerr << "ERR: Error receiving message, exiting receiver thread" << endl;
			break;
		}

		if (message != nullptr)
		{
			// cerr << "Received message: " << message << endl;
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

	// cerr << "Receiver thread exiting" << endl;
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
		cerr << "ERR: Error setting socket timeout." << endl;
		close(sock);
		exit(1);
		// Handle error, maybe return or exit
	}
}

void TcpUdp::signalHandlerThread(const sigset_t &signals)
{
	struct timespec zero_timeout = {0, 0};
	siginfo_t info;
	// cerr << "Starting signal handler thread" << endl;

	// sigset_t sigset;
	// sigemptyset(&sigset);
	// for (int sig : signals) {
	//     sigaddset(&sigset, sig);
	// }
	// pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

	int sig;
	// while (true) {
	while (inputBuffer.getNetwork() == true)
	{
		// cerr << "Waiting for signal..." << endl;

		// int res = sigwait(&signals, &sig);
		sig = sigtimedwait(&signals, &info, &zero_timeout);
		if (sig > 0)
		{
			// printf("Signal %d received\n", sig);
			if (sig == SIGINT)
			{
				// std::cout << "SIGINT received, exiting.\n";

				inputBuffer.setNetwork(false);

				if (inputBuffer.protocol == InputBuffer::Protocol::UDP)
				{
					sendByeUDP(SentID);
				}
				else
				{
					sendMessageTCP("BYE\r\n");
				}

				break;
			}
		}
		else if (errno == EAGAIN)
		{
			// printf("No signal received\n");
		}
		else
		{
			perror("sigtimedwait");
		}

		// if (res == 0)
		// if (sig == 0)
		// {
		// 	// std::cout << "Signal received: " << sig << std::endl;
		// 	// Handle signal
		// 	if (sig == SIGINT)
		// 	{
		// 		// std::cout << "SIGINT received, exiting.\n";

		// 		inputBuffer.setNetwork(false);

		// 		if (inputBuffer.protocol == InputBuffer::Protocol::UDP)
		// 		{
		// 			sendByeUDP(SentID);
		// 		}
		// 		else
		// 		{
		// 			sendMessageTCP("BYE\r\n");
		// 		}

		// 		break;
		// 	}
		// }
		// else
		// {
		// 	// std::cerr << "sigwait error: " << res << std::endl;
		// }

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// cerr << "Signal handler thread exiting" << endl;
}

void TcpUdp::Input(int Protocol)
{

	// cerr << "Starting threads" << endl;
	// InputBuffer inputBuffer;
	// ResponseBuffer responseBuffer;
	sigset_t mySigSet; // Use a clear and distinct name for the variable
	sigemptyset(&mySigSet);
	sigaddset(&mySigSet, SIGINT);
	// Block SIGINT in the main thread.
	pthread_sigmask(SIG_BLOCK, &mySigSet, NULL);

	if (Protocol == 1)
	{
		inputBuffer.protocol = InputBuffer::Protocol::TCP;
		cerr << "Connecting to server..." << endl;
		connectToServer();

		inputBuffer.setNetwork(true);

		// TcpUdp *TcpUdp::instance = nullptr;
		// signal(SIGINT, TcpUdp::signalHandler);

		std::thread inputThread(&TcpUdp::stdinReaderThread, this);
		std::thread networkThread(&TcpUdp::networkCommunicationThread, this);
		std::thread signalHandlerThread(&TcpUdp::signalHandlerThread, this, std::ref(mySigSet));

		// thread inputThread([this, &inputBuffer] { this->stdinReaderThread(inputBuffer); });
		// thread networkThread([this, &inputBuffer, &responseBuffer] { this->networkCommunicationThread(inputBuffer, responseBuffer); });

		inputThread.join();
		networkThread.join();
		signalHandlerThread.join();
	}
	else
	{
		// setSocketTimeout(sock, UdpTimeout);
		inputBuffer.protocol = InputBuffer::Protocol::UDP;
		// cerr << "Starting threads" << endl;
		// TcpUdp *TcpUdp::instance = nullptr;
		// signal(SIGINT, TcpUdp::signalHandler);
		// signal(SIGINT, signalHandler);

		int flags = fcntl(sock, F_GETFL, 0);
		if (flags == -1)
		{
			// FIXME: Handle err
			//  Handle error
		}
		flags = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
		if (flags == -1)
		{
			// FIXME: Handle err
			//  Handle error
		}

		inputBuffer.setNetwork(true);

		// cerr << "Starting threads" << endl;
		// cerr << "Starting input thread" << endl;
		// cerr << "Starting network thread" << endl;

		thread signalHandlerThread(&TcpUdp::signalHandlerThread, this, std::ref(mySigSet));

		// std::this_thread::sleep_for(std::chrono::milliseconds(10));

		thread inputThread(&TcpUdp::stdinReaderThread, this);
		thread networkThread(&TcpUdp::networkCommunicationThread, this);
		thread receiverThread(&TcpUdp::receiverThread, this);

		// thread inputThread([this, &inputBuffer] { this->stdinReaderThread(inputBuffer); });
		// thread networkThread([this, &inputBuffer, &responseBuffer] { this->networkCommunicationThread(inputBuffer, responseBuffer); });
		// thread receiverThread([this, &inputBuffer, &responseBuffer] { this->receiverThread(inputBuffer, responseBuffer); });

		signalHandlerThread.join();
		inputThread.join();
		networkThread.detach();
		// networkThread.join();
		receiverThread.join();

		// std::unique_lock<std::mutex> lk(networkMutex);
		// if(networkThread.wait_for(lk, std::chrono::seconds(2), []{ return networkThreadReady; })) {

		// } else {
		//     // Timeout expired
		//     std::cout << "Timeout expired, worker thread did not finish.\n";
		//     // Here, you decide on the strategy since you can't safely "kill" the thread.
		//     // Often, this would involve setting another flag that the worker checks to terminate early
		//     // or taking other application-specific recovery actions.
		// }
	}

	// char* text = memoryManager.allocate<char>(1500);
	// // text = "Hello";
	// std::strcpy(text, "Hello");

	// cerr << "Sending message: " << text << endl;

	// memoryManager.freeSpecific(text);

	// cerr << "Exiting" << endl;
}
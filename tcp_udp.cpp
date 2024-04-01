#include "tcp_udp.h"

TcpUdp::TcpUdp(sockaddr_in server_address, int sock, unsigned short UdpTimeout, unsigned short UdpRetransmissions)
		: server_address(server_address), sock(sock), UdpTimeout(UdpTimeout), UdpRetransmissions(UdpRetransmissions) {}

void TcpUdp::connectToServer()
{
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
}

void TcpUdp::sendMessageTCP(const string &message)
{
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
}

void TcpUdp::printMessageAsHex(const char *message, size_t length)
{
	// std::cerr << "Sending message: ";
	for (size_t i = 0; i < length; ++i)
	{
		cerr << std::hex << std::setw(2) << std::setfill('0')
							<< static_cast<int>(static_cast<unsigned char>(message[i])) << " ";
	}
	cerr << endl;
}

void TcpUdp::confirmThread(short expectedID, const char *message, size_t length)
{
	std::lock_guard<std::mutex> lock(confirmMutex);

	auto Timeout = chrono::milliseconds(UdpTimeout);
	auto checkInterval = chrono::milliseconds(UdpTimeout / 10);

	InputProcess inputProcess;
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
		}
		sendingMutex.unlock();

		auto startTime = chrono::steady_clock::now();
		chrono::steady_clock::time_point checkTime = startTime;

		while (chrono::steady_clock::now() - startTime < Timeout)
		{

			this_thread::sleep_until(checkTime + checkInterval);
			checkTime = chrono::steady_clock::now();

			std::lock_guard<std::mutex> guard(bufferMutex);
			vector<unsigned char> check_ID = inputProcess.udp_construct_confirm(expectedID);

			const char *response = responseBuffer
																 .getMessageFromBuffer(reinterpret_cast<const char *>(check_ID.data()), check_ID.size());

			if (response != nullptr)
			{
				responseBuffer
						.removeMessageFromBuffer(reinterpret_cast<const char *>(check_ID.data()), check_ID.size());
				success = true;
				break;
			}
		}

		if (success)
		{
			setReceived(true);
			confirmCondition.notify_one();
			return;
		}
		else
		{
			cerr << "ERR: Timeout occurred while waiting for a message, retrying..." << endl;
		}
	}

	if (!success)
	{

		cerr << "ERR: Packet lost, retransmission limit reached" << endl;
		sendByeUDP(SentID);

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

	if (-1 == sendto(sock, message, length, 0, (struct sockaddr *)&server_address, sizeof(server_address)))
	{
		cerr << ERR_ << "Could not send confirmation" << endl;
	}

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
	}
	sendingMutex.unlock();
}

void TcpUdp::sendMessageUDP(const char *message, size_t length)
{

	if (sock == -1)
	{
		cerr << ERR_ << "Socket is closed" << endl;
		exit(1);
	}

	auto messageCopy = std::make_unique<char[]>(length);
	std::memcpy(messageCopy.get(), message, length);

	std::thread confirmThread([this, messageCopy = std::move(messageCopy), length]() mutable
														{ this->confirmThread(this->SentID, messageCopy.get(), length); });

	confirmThread.detach();
}

string TcpUdp::receiveMessageTCP()
{
	char buffer[1500];

	int bytesReceived = recv(sock, buffer, 1500, 0);

	if (bytesReceived == -1)
	{
		return "";
	// 	cerr << ERR_ << "Error in recv(). Quitting" << endl;
	// 	close(sock);
	// 	exit(4);
	}

	if (bytesReceived == 0)
	{
		cerr << ERR_ << "Client disconnected " << endl;
		// inputBuffer.setNetwork(false);
		return "";
		// close(sock);
		// exit(5);
	}

	return string(buffer, bytesReceived);
}

char *TcpUdp::receiveMessageUDP(int *err, int *bytes)
{
	char *buffer = new char[1500];

	struct sockaddr_in senderAddr;
	socklen_t senderAddrLen = sizeof(senderAddr);
	memset(&senderAddr, 0, sizeof(senderAddr));

	int bytesReceived = recvfrom(sock, buffer, 1500, 0, (struct sockaddr *)&senderAddr, &senderAddrLen);

	if (bytesReceived == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{

			delete[] buffer;
			*err = 1;
			return nullptr;
		}
		else
		{
			cerr << ERR_ << "Error in recvfrom()." << endl;
			delete[] buffer;
			*err = 2;
			return nullptr;
		}
	}

	*bytes = bytesReceived;

	server_address.sin_port = senderAddr.sin_port;

	return buffer;
}

void TcpUdp::closeConnection()
{
	close(sock);
}

void TcpUdp::stdinReaderThread()
{

	string line;
	while (inputBuffer.getNetwork() == true)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);

		timeval timeout;
		timeout.tv_sec = 0;				// Seconds
		timeout.tv_usec = 100000; // Microseconds

		int ret = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
		if (ret > 0)
		{
			if (FD_ISSET(STDIN_FILENO, &readfds))
			{
				getline(cin, line);
				if (line.empty())
				{
					continue;
				}

				inputBuffer.addLine(line);
			}
		}
		else if (ret == 0)
		{
			if (inputBuffer.getNetwork() == false)
			{
				break;
			}
		}
		else
		{
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

void TcpUdp::networkCommunicationThread()
{
	string ServerName = "Server";
	InputProcess inputProcess;

	SentID = 0;

	InputBuffer::Protocol Protocol = inputBuffer.protocol;

	while (inputBuffer.getNetwork() == true)
	{
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
						setReceived(false);

						cout << DisplayName << ": " << message.message << endl;

						short errID = 0xFE;
						short byeID = 0xFF;

						const char *err = nullptr;
						size_t err_length = 0;

						const char *bye = nullptr;
						size_t bye_length = 0;

						tie(err, err_length) = responseBuffer.getMessageByIDFromBuffer(errID);
						if (err != nullptr)
						{
							unsigned short msgID = (static_cast<unsigned char>(err[1]) << 8) | static_cast<unsigned char>(err[2]);

							sendConfirm(msgID);
							const char *start = &err[3];
							const char *end = std::strchr(start, '\0');

							if (end == nullptr)
							{
								cerr << "ERR: Server name is not found" << endl;
								sendByeUDP(SentID);
								responseBuffer.removeMessageFromBuffer(err, err_length);
								break;
							}

							ServerName = string(start, end - start);
							const char *nextStart = end + 1;
							const char *nextEnd = std::strchr(nextStart, '\0');

							string reply_msg = string(nextStart, nextEnd - nextStart);

							if (!inputProcess.isValidDName(ServerName))
							{
								// cerr << "ERR: Invalid server name" << endl;
								// sendByeUDP(SentID);
								// responseBuffer.removeMessageFromBuffer(err, err_length);
								handleErrFromServerUDP();
								break;
							}

							if (!inputProcess.isValidContent(reply_msg))
							{
								// cerr << "ERR: Invalid message" << endl;
								// sendByeUDP(SentID);
								// responseBuffer.removeMessageFromBuffer(err, err_length);
								handleErrFromServerUDP();
								break;
							}

							cerr << "ERR FROM " << ServerName << ": " << reply_msg << endl;

							sendByeUDP(SentID);
							responseBuffer.removeMessageFromBuffer(err, err_length);
							break;
						}

						tie(bye, bye_length) = responseBuffer.getMessageByIDFromBuffer(byeID);
						if (bye != nullptr)
						{
							unsigned short msgID = (static_cast<unsigned char>(bye[1]) << 8) | static_cast<unsigned char>(bye[2]);

							sendConfirm(msgID);

							sendByeUDP(SentID);
							responseBuffer.removeMessageFromBuffer(bye, bye_length);
							break;
						}

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

						auto messageBytes = inputProcess.udp_construct_auth(message, SentID);
						const char *sequence = reinterpret_cast<const char *>(messageBytes.data());

						sendMessageUDP(sequence, messageBytes.size());

						unique_lock<mutex> lock(confirmMutex);
						confirmCondition.wait(lock, [this]
																	{ return confirmReceived; });
						setReceived(false);

						char ReplyID = 0x01;
						char ErrorID = 0xFE;
						const char *reply = nullptr;
						size_t rep_length = 0;
						const char *error = nullptr;
						size_t error_length = 0;

						int what_to_do = 0; // 1 - continue, 2 - break

						chrono::milliseconds timeout(UdpTimeout);
						chrono::steady_clock::time_point start = chrono::steady_clock::now();

						while (what_to_do == 0)
						{
							tie(reply, rep_length) = responseBuffer.getMessageByIDFromBuffer(ReplyID);

							if (reply != nullptr)
							{

								const char result = reply[3];
								unsigned short msgID = (static_cast<unsigned char>(reply[1]) << 8) | static_cast<unsigned char>(reply[2]);

								ReceivedID = msgID;

								sendConfirm(ReceivedID);

								if (result == 0x01)
								{
									CurrentStatus = OPEN;

									const char *start = &reply[6];
									const char *end = std::strchr(start, '\0');

									if (end == nullptr)
									{
										cerr << "ERR: undefined message in reply" << endl;
										what_to_do = 2;
										break;
									}

									string reply_msg = string(start, end - start);

									if (!inputProcess.isValidContent(reply_msg))
									{
										// cerr << "ERR: Invalid message" << endl;
										// sendByeUDP(SentID);
										handleErrFromServerUDP();
										what_to_do = 2; // break
										break;
									}

									cerr << "Success: " << reply_msg << endl;

									responseBuffer.removeMessageFromBuffer(reply, rep_length);

									what_to_do = 1; // continue
								}
								else if (result == 0x00)
								{

									const char *start = &reply[6];							// Start from the 3rd byte
									const char *end = std::strchr(start, '\0'); // Find the next zero byte

									if (end == nullptr)
									{
										// cerr << "ERR: Undefined message in reply" << endl;
										handleErrFromServerUDP();

										what_to_do = 2; // continue
										break;
									}

									string reply_msg = string(start, end - start);

									if (!inputProcess.isValidContent(reply_msg))
									{
										// cerr << "ERR: Invalid message" << endl;
										// sendByeUDP(SentID);
										handleErrFromServerUDP();

										what_to_do = 2; // break
										break;
									}

									cerr << "Failure: " << reply_msg << endl;

									CurrentStatus = START;
									responseBuffer.removeMessageFromBuffer(reply, rep_length);

									what_to_do = 1; // continue
								}
								else
								{
									if (!inputProcess.isValidDName(ServerName)){
										handleErrFromServerUDP();
										what_to_do = 2; // break
										break;
									}

									cerr << "ERR FROM " << ServerName << ": "
											 << "Unknown reply" << endl;
									CurrentStatus = START;
									responseBuffer.removeMessageFromBuffer(reply, rep_length);
									what_to_do = 1; // continue
								}
							}

							tie(error, error_length) = responseBuffer.getMessageByIDFromBuffer(ErrorID);

							if (error != nullptr)
							{
								unsigned short msgID = (static_cast<unsigned char>(error[1]) << 8) | static_cast<unsigned char>(error[2]);

								sendConfirm(msgID);
								const char *start = &error[3];
								const char *end = std::strchr(start, '\0');

								if (end == nullptr)
								{
									cerr << "ERR: Server name is not found" << endl;
									sendByeUDP(SentID);
									what_to_do = 2; // break
								}

								ServerName = string(start, end - start);
								const char *nextStart = end + 1;
								const char *nextEnd = std::strchr(nextStart, '\0');

								string err_msg = string(nextStart, nextEnd - nextStart);

								if (!inputProcess.isValidDName(ServerName))
								{
									// cerr << "ERR: Invalid server name" << endl;
									// sendByeUDP(SentID);
										handleErrFromServerUDP();

									what_to_do = 2; // break
									break;
								}

								if (!inputProcess.isValidContent(err_msg))
								{
									// cerr << "ERR: Invalid message" << endl;
									// sendByeUDP(SentID);
										handleErrFromServerUDP();

									what_to_do = 2; // break
									break;
								}


								cerr << "ERR FROM " << ServerName << ": " << err_msg << endl;

								sendByeUDP(SentID);
								responseBuffer.removeMessageFromBuffer(error, error_length);
								what_to_do = 2; // break
							}

							if (what_to_do == 1 || what_to_do == 2)
								break;

							std::this_thread::sleep_for(std::chrono::milliseconds(UdpTimeout / 10));

							if (chrono::steady_clock::now() - start > timeout)
							{
								cerr << "ERR: Timeout of reply reached" << endl;
								sendByeUDP(SentID);
								what_to_do = 2; // break
																// break;
							}
						}

						std::this_thread::sleep_for(std::chrono::milliseconds(300));

						tie(error, error_length) = responseBuffer.getMessageByIDFromBuffer(ErrorID);

						if (error != nullptr)
						{
							unsigned short msgID = (static_cast<unsigned char>(error[1]) << 8) | static_cast<unsigned char>(error[2]);

							sendConfirm(msgID);
							const char *start = &error[3];
							const char *end = std::strchr(start, '\0');

							if (end == nullptr)
							{
								// cerr << "ERR: Server name is not found" << endl;
								// sendByeUDP(SentID);
										handleErrFromServerUDP();

								what_to_do = 2; // break
								break;
							}

							ServerName = string(start, end - start);
							const char *nextStart = end + 1;
							const char *nextEnd = std::strchr(nextStart, '\0');

							string err_msg = string(nextStart, nextEnd - nextStart);
							if (!inputProcess.isValidDName(ServerName))
							{
								// cerr << "ERR: Invalid server name" << endl;
								// sendByeUDP(SentID);
										handleErrFromServerUDP();

								what_to_do = 2; // break
								break;
							}

							if (!inputProcess.isValidContent(err_msg))
							{
								// cerr << "ERR: Invalid message" << endl;
								// sendByeUDP(SentID);
										handleErrFromServerUDP();

								what_to_do = 2; // break
								break;
							}

							cerr << "ERR FROM " << ServerName << ": " << err_msg << endl;

							sendByeUDP(SentID);
							responseBuffer.removeMessageFromBuffer(error, error_length);
							what_to_do = 2; // break
						}

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

						string respond = "";
						// string respond = receiveMessageTCP();

						std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
						std::chrono::milliseconds timeout(UdpTimeout*10);

						while (respond == "") {
							// responseBuffer.printBufferTCP();
							respond = responseBuffer.getMessageFromBufferTCP("REPLY");
							if (respond == "") 
								respond = responseBuffer.getMessageFromBufferTCP("ERR");
							// cerr << "Responddd: " << respond << endl;

							if (inputProcess.parseRespondAuth(respond))
							{
								// cerr << "Respond: " << respond << endl;
								vector<string> vector = inputProcess.splitString(respond, ' ');
								string messageContent = inputProcess.extractMessageContent(respond);

								if (vector[1] == "OK")
								{
									CurrentStatus = OPEN;
									cerr << "Success: " << messageContent << endl;

									string err = "";
									err = responseBuffer.getMessageFromBufferTCP("ERR");
									if (err != "")
									{
										// vector<string > vector = inputProcess.splitString(err, ' ');
										std::vector<std::string> vector = inputProcess.splitString(err, ' ');

										std::string messageContent = inputProcess.extractMessageContent(err);


										cerr << "ERR FROM " << DisplayName << ": " << messageContent << endl;
										sendMessageTCP("BYE\r\n");
										break;
									}
								}
								else
								{
									CurrentStatus = START;
									cerr << "Failure: " << messageContent << endl;

									string err = responseBuffer.getMessageFromBufferTCP("ERR");
									if (err != "")
									{
										std::vector<std::string> vector = inputProcess.splitString(err, ' ');
										std::string messageContent = inputProcess.extractMessageContent(err);

										cerr << "ERR FROM " << DisplayName << ": " << messageContent << endl;
										sendMessageTCP("BYE\r\n");
										break;
									}
									continue;
								}
							}
							else
							{
								if (respond == "") {
									std::this_thread::sleep_for(std::chrono::milliseconds(100));
									if (std::chrono::steady_clock::now() - start > timeout) {
										cerr << "ERR: Timeout of reply reached" << endl;
										inputBuffer.setNetwork(false);
										sendMessageTCP("BYE\r\n");
										break;
									}
									continue;
								}
								// cerr << "ERR FROM " << DisplayName << ":: " << messageContent << endl;
								string messageContent = inputProcess.extractMessageContent(respond);
								if (!inputProcess.isValidContent(messageContent))
								{
									// cerr << "ERR: Invalid message" << endl;
									sendMessageTCP("BYE\r\n");
									break;
								}

								vector<string> vector = inputProcess.splitString(respond, ' ');
								ServerName = vector[2];
								if (!inputProcess.isValidDName(ServerName))
								{
									// cerr << "ERR: Invalid server name" << endl;
									sendMessageTCP("BYE\r\n");
									break;
								}

								cerr << "ERR FROM " << ServerName << ": " << messageContent << endl;
								sendMessageTCP("BYE\r\n");
								break;
							}

						}
					}
				}

				if (message.type == InputProcess::Command::JOIN)
				{

					if (CurrentStatus != OPEN)
					{
						cerr << ERR_ << "You need to authenticate first" << endl;
						continue;
					}

					message.displayName = DisplayName;

					if (Protocol == InputBuffer::Protocol::UDP)
					{

						auto messageBytes = inputProcess.udp_construct_join(message, SentID);
						const char *sequence = reinterpret_cast<const char *>(messageBytes.data());
						size_t length = messageBytes.size();

						sendMessageUDP(sequence, length);

						unique_lock<mutex> lock(confirmMutex);
						confirmCondition.wait(lock, [this]
																	{ return confirmReceived; });
						setReceived(false);

						char ReplyID = 0x01;

						const char *reply = nullptr;
						size_t reply_length = 0;
						unsigned short msgID = 0;

						const char *err = nullptr;
						size_t err_length = 0;

						const char *bye = nullptr;
						size_t bye_length = 0;

						char rep_result = 0x00;

						bool is_break = false;

						while (reply == nullptr)
						{
							if (reply == nullptr)
							{
								tie(reply, reply_length) = responseBuffer.getMessageByIDFromBuffer(ReplyID);
								if (reply != nullptr)
								{
									rep_result = reply[3];
									// if (rep_result == 0x00)
									// {
									rep_result = reply[3];
									msgID = (static_cast<unsigned char>(reply[1]) << 8) | static_cast<unsigned char>(reply[2]);
									ReceivedID = msgID;
									sendConfirm(ReceivedID);
									break;
									// }
									// else
									// {
									// 	rep_result = reply[3];
									// 	msgID = (static_cast<unsigned char>(reply[1]) << 8) | static_cast<unsigned char>(reply[2]);
									// 	ReceivedID = msgID;
									// 	sendConfirm(ReceivedID);
									// }
								}
							}

							if (err == nullptr)
							{
								tie(err, err_length) = responseBuffer.getMessageByIDFromBuffer(0xFE);
							}
							else
							{
								unsigned short msgID = (static_cast<unsigned char>(err[1]) << 8) | static_cast<unsigned char>(err[2]);
								sendConfirm(msgID);
								const char *start = &err[3];
								const char *end = std::strchr(start, '\0');

								if (end == nullptr)
								{
									cerr << "ERR: Server name is not found" << endl;
									is_break = true;
									sendByeUDP(SentID);
									responseBuffer.removeMessageFromBuffer(err, err_length);
									is_break = true;
								}

								ServerName = string(start, end - start);
								const char *nextStart = end + 1;
								const char *nextEnd = std::strchr(nextStart, '\0');

								string err_msg = string(nextStart, nextEnd - nextStart);

								if (!inputProcess.isValidDName(ServerName))
								{
									handleErrFromServerUDP();
									is_break = true;
									break;
								}
								if (!inputProcess.isValidContent(err_msg))
								{
									handleErrFromServerUDP();
									is_break = true;
									break;
								}

								cerr << "ERR FROM " << ServerName << ": " << err_msg << endl;

								sendByeUDP(SentID);
								responseBuffer.removeMessageFromBuffer(err, err_length);
								is_break = true;
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
								responseBuffer.removeMessageFromBuffer(bye, bye_length);
								is_break = true;
								break;
							}
						}

						if (is_break)
							break;

						if (rep_result == 0x01)
						{
							const char *rep_start = &reply[6];									// Start from the 3rd byte
							const char *rep_end = std::strchr(rep_start, '\0'); // Find the next zero byte

							string reply_msg = string(rep_start, rep_end - rep_start);

							cerr << "Success: " << reply_msg << endl;
							responseBuffer.removeMessageFromBuffer(reply, reply_length);

							continue;
						}
						else if (rep_result == 0x00)
						{
							const char *rep_start = &reply[6];
							const char *rep_end = std::strchr(rep_start, '\0');

							string reply_msg = string(rep_start, rep_end - rep_start);

							cerr << "Failure: " << reply_msg << endl;
							responseBuffer.removeMessageFromBuffer(reply, reply_length);

							continue;
						}
						else
						{

							if (!inputProcess.isValidDName(ServerName)) {
								handleErrFromServerUDP();
								break;
							}

							cerr << "ERR FROM " << ServerName << ": "
									 << "Unknown reply" << endl;
							sendByeUDP(SentID);
							responseBuffer.removeMessageFromBuffer(reply, reply_length);
							break;
						}
					}
					else if (Protocol == InputBuffer::Protocol::TCP)
					{

						string message_to_send = inputProcess.tcp_construct_join(message);

						sendMessageTCP(message_to_send);

						// string respond = receiveMessageTCP();
						string respond = "";
						// string respond = receiveMessageTCP();

						std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
						std::chrono::milliseconds timeout(UdpTimeout*10);
						bool is_break = false;

						while (respond == "") {
							// responseBuffer.printBufferTCP();
							respond = responseBuffer.getMessageFromBufferTCP("REPLY");
							if (respond == "") 
								respond = responseBuffer.getMessageFromBufferTCP("ERR");
							// cerr << "Responddd: " << respond << endl;

							if (inputProcess.parseRespondAuth(respond))
							{
								// cerr << "Respond: " << respond << endl;
								vector<string> vector = inputProcess.splitString(respond, ' ');
								string messageContent = inputProcess.extractMessageContent(respond);

								if (!inputProcess.isValidContent(messageContent))
								{
									// cerr << "ERR: Invalid message" << endl;
									sendMessageTCP("BYE\r\n");
									is_break = true;
									break;
								}

								if (vector[1] == "OK")
								{
									CurrentStatus = OPEN;
									cerr << "Success: " << messageContent << endl;

									// string err = "";
									// err = responseBuffer.getMessageFromBufferTCP("ERR");
									// if (err != "")
									// {
									// 	// vector<string > vector = inputProcess.splitString(err, ' ');
									// 	std::vector<std::string> vector = inputProcess.splitString(err, ' ');

									// 	std::string messageContent = inputProcess.extractMessageContent(err);


									// 	cerr << "ERR FROM " << DisplayName << ": " << messageContent << endl;
									// 	sendMessageTCP("BYE\r\n");
									// 	break;
									// }
								}
								else
								{
									CurrentStatus = START;
									cerr << "Failure: " << messageContent << endl;

									// string err = responseBuffer.getMessageFromBufferTCP("ERR");
									// if (err != "")
									// {
									// 	std::vector<std::string> vector = inputProcess.splitString(err, ' ');
									// 	std::string messageContent = inputProcess.extractMessageContent(err);

									// 	cerr << "ERR FROM " << DisplayName << ": " << messageContent << endl;
									// 	sendMessageTCP("BYE\r\n");
									// 	break;
									// }
									continue;
								}
							}
							else
							{
								if (respond == "") {
									std::this_thread::sleep_for(std::chrono::milliseconds(100));
									if (std::chrono::steady_clock::now() - start > timeout) {
										cerr << "ERR: Timeout of reply reached" << endl;
										inputBuffer.setNetwork(false);
										sendMessageTCP("BYE\r\n");
										break;
									}
									continue;
								}
								// cerr << "ERR FROM " << DisplayName << ":: " << messageContent << endl;
								string messageContent = inputProcess.extractMessageContent(respond);

								vector<string> vector = inputProcess.splitString(respond, ' ');
								ServerName = vector[2];

								if (!inputProcess.isValidDName(ServerName))
								{
									// handleErrFromServerUDP();
									sendMessageTCP("BYE\r\n");
									is_break = true;
									break;
								}

								if (!inputProcess.isValidContent(messageContent))
								{
									// handleErrFromServerUDP();
									sendMessageTCP("BYE\r\n");
									is_break = true;
									break;
								}

								cerr << "ERR FROM " << ServerName << ": " << messageContent << endl;
								sendMessageTCP("BYE\r\n");
								break;
							}

						}

						if (is_break)
							break;
					}
				}
			}
		}
		if (cin.eof())
		{
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

	inputBuffer.setNetwork(false);
}

void TcpUdp::setReceived(bool received)
{
	lock_guard<mutex> lock(confirmReceivedMutex);
	confirmReceived = received;
}

void TcpUdp::handleErrFromServerUDP() {
	InputProcess inputProcess;
	auto messageBytes = inputProcess.udp_construct_err(SentID, DisplayName);

	const char *sequence = reinterpret_cast<const char *>(messageBytes.data());
	sendMessageUDP(sequence, messageBytes.size());

	cerr << "ERR: Invalid message received, exiting receiver thread" << endl;

	unique_lock<mutex> lock(confirmMutex);
	confirmCondition.wait(lock, [this]
												{ return confirmReceived; });
	setReceived(false);

	sendByeUDP(SentID);
	inputBuffer.setNetwork(false);
}

void TcpUdp::receiverThread()
{
	while (inputBuffer.getNetwork() == true)
	{

		int err = 0;
		int bytes = 0;
		const char *message = receiveMessageUDP(&err, &bytes);

		if (message != nullptr)
		{
			const unsigned char firstByte = message[0];
			if (firstByte != 0xFF && firstByte != 0xFE && firstByte != 0x00 && firstByte != 0x01 &&
					firstByte != 0x02 && firstByte != 0x03 && firstByte != 0x04)
			{

				short MsgID = message[1] << 8 | message[2];
				sendConfirm(MsgID);

				handleErrFromServerUDP();
				// InputProcess inputProcess;
				// auto messageBytes = inputProcess.udp_construct_err(SentID, DisplayName);

				// const char *sequence = reinterpret_cast<const char *>(messageBytes.data());
				// sendMessageUDP(sequence, messageBytes.size());

				// cerr << "ERR: Invalid message received, exiting receiver thread" << endl;

				// unique_lock<mutex> lock(confirmMutex);
				// confirmCondition.wait(lock, [this]
				// 											{ return confirmReceived; });
				// setReceived(false);

				// sendByeUDP(SentID);
				// inputBuffer.setNetwork(false);
				break;
			}
		}

		if (message != nullptr)
		{
			const unsigned char firstByte = message[0];
			InputProcess inputProcess;

			if (firstByte == 0x04)
			{
				short MsgID = message[1] << 8 | message[2];
				sendConfirm(MsgID);

				const char *start = &message[3];
				const char *end = std::strchr(start, '\0');

				if (end == nullptr)
				{
					cerr << "ERR: Server name is not found" << endl;
					sendByeUDP(SentID);
					break;
				}

				string ServerName = string(start, end - start);
				// if (ServerName.length() > 20 && !inputProcess.isValidDName(ServerName))
				if (!inputProcess.isValidDName(ServerName))
				{
					// cerr << "ERR: Server name is too long" << endl;

					// auto messageBytes = inputProcess.udp_construct_err(SentID, DisplayName);

					// const char *sequence = reinterpret_cast<const char *>(messageBytes.data());
					// sendMessageUDP(sequence, messageBytes.size());

					// cerr << "ERR: Invalid message received, exiting receiver thread" << endl;

					// unique_lock<mutex> lock(confirmMutex);
					// confirmCondition.wait(lock, [this]
					// 											{ return confirmReceived; });
					// setReceived(false);

					// sendByeUDP(SentID);
					// inputBuffer.setNetwork(false);
					handleErrFromServerUDP();
					break;
					// sendByeUDP(SentID);
					// break;
				}
				const char *nextStart = end + 1;
				const char *nextEnd = std::strchr(nextStart, '\0');

				string reply_msg = string(nextStart, nextEnd - nextStart);
				// if (reply_msg.length() > 1400 && !inputProcess.isValidContent(reply_msg))
				if (!inputProcess.isValidContent(reply_msg))
				{
					handleErrFromServerUDP();
					// auto messageBytes = inputProcess.udp_construct_err(SentID, DisplayName);

					// const char *sequence = reinterpret_cast<const char *>(messageBytes.data());
					// sendMessageUDP(sequence, messageBytes.size());

					// cerr << "ERR: Invalid message received, exiting receiver thread" << endl;

					// unique_lock<mutex> lock(confirmMutex);
					// confirmCondition.wait(lock, [this]
					// 											{ return confirmReceived; });
					// setReceived(false);

					// sendByeUDP(SentID);
					// inputBuffer.setNetwork(false);
					break;
					// sendByeUDP(SentID);
					// break;
				}

				cout << ServerName << ": " << reply_msg << endl;
				continue;
			}
		}

		if (err == 2)
		{
			cerr << "ERR: Error receiving message, exiting receiver thread" << endl;
			break;
		}

		if (message != nullptr)
		{
			responseBuffer.addMessageToBuffer(message, bytes);
		}

		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	responseBuffer.clearBuffer();
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

	int sig;
	while (inputBuffer.getNetwork() == true)
	{
		sig = sigtimedwait(&signals, &info, &zero_timeout);
		if (sig > 0)
		{
			if (sig == SIGINT)
			{

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
			perror("ERR: sigtimedwait");
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void TcpUdp::receiverThreadTCP()
{
	InputProcess inputProcess;

	while (inputBuffer.getNetwork() == true)
	{
		string response = receiveMessageTCP();
		response = inputProcess.upperTCP(response);

		if (!response.empty()) {
			string check = inputProcess.splitString(response, ' ')[0];
			// cerr << "Check: " << check << endl;
			if (check != "ERR" && check != "REPLY" && check != "BYE" && check != "MSG")
			{
				// cerr << "CHECK: " << check << endl;
				// cerr << "ERR: Invalid message received, exiting receiver thread" << endl;
				string message = "Invalid message";
				string err = "ERR FROM " + DisplayName + " IS " + message + "\r\n";
				// cerr << err << endl;
				sendMessageTCP(err);
				cerr << "ERR: Invalid message received, exiting receiver thread" << endl;
				sendMessageTCP("BYE\r\n");
				inputBuffer.setNetwork(false);
				break;
			}
		}

		if (response.empty())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		vector<string> vector = inputProcess.splitString(response, ' ');

		if (vector[0] == "MSG" && inputProcess.parseMSG(response))
		{
			string messageContent = inputProcess.extractMessageContent(response);
			cout << vector[2] << ": " << messageContent << endl;
			continue;
		}

		if (vector[0] == "ERR" && inputProcess.parseERR(response))
		{
			string messageContent = inputProcess.extractMessageContent(response);
			cerr << "ERR FROM " << vector[2] << ": " << messageContent << endl;
			sendMessageTCP("BYE\r\n");
			inputBuffer.setNetwork(false);
			// continue;
			break;
		}
		

		responseBuffer.addMessageToBuffer(response);

	}
}

void TcpUdp::Input(int Protocol)
{

	sigset_t mySigSet;
	sigemptyset(&mySigSet);
	sigaddset(&mySigSet, SIGINT);
	pthread_sigmask(SIG_BLOCK, &mySigSet, NULL);



	if (Protocol == 1)
	{
		inputBuffer.protocol = InputBuffer::Protocol::TCP;
		// cerr << "Connecting to server..." << endl;
		connectToServer();

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

		std::thread inputThread(&TcpUdp::stdinReaderThread, this);
		std::thread networkThread(&TcpUdp::networkCommunicationThread, this);
		std::thread signalHandlerThread(&TcpUdp::signalHandlerThread, this, std::ref(mySigSet));
		std::thread receiverThread(&TcpUdp::receiverThreadTCP, this);

		inputThread.join();
		// networkThread.join();
		networkThread.detach();
		receiverThread.join();
		signalHandlerThread.join();
	}
	else
	{
		inputBuffer.protocol = InputBuffer::Protocol::UDP;

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

		thread signalHandlerThread(&TcpUdp::signalHandlerThread, this, std::ref(mySigSet));

		thread inputThread(&TcpUdp::stdinReaderThread, this);
		thread networkThread(&TcpUdp::networkCommunicationThread, this);
		thread receiverThread(&TcpUdp::receiverThread, this);

		signalHandlerThread.join();
		inputThread.join();
		networkThread.detach();
		receiverThread.join();
	}
}
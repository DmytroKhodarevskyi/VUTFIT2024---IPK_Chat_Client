# IPK24CHAT-CLIENT

## Table of Contents

- [IPK24CHAT-CLIENT](#ipk24chat-client)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Getting Started](#getting-started)
    - [Prerequisites](#prerequisites)
    - [Installation](#installation)
  - [Testing](#testing)
  - [License](#license)
  - [Acknowledgements](#acknowledgements)

## Introduction

This is a simple client for messaging service. It uses both UDP and TCP protocols to send and receive messages. The client is written in C++ and uses the `unix socket` framework to communicate with the server.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.


### Prerequisites

C++ compiler and `make` utility are required to build the project. The project is tested on `g++` compiler.

The client itself desired to run on **Linux OS**. The project was tested on **Ubuntu**.

### Installation

1. Clone the repo
   ```sh
   git clone https://git.fit.vutbr.cz/xkhoda01/IPK_1st_project.git
    ```
2. Build the project
    ```sh
    make
    ```
3. Run the client
    ```sh
    ./ipk24-chat-client.exe -t <protocol> -s <server_ip> -p <server_port> 
    ```

## Usage

The client can be used to send and receive messages from the server. The client supports the following commands:

- `/auth Username DisplayName Secret` - Authenticate the user with the server. `Username` is your login, `DisplayName` will be your Nickname, `Secret` is token for authentication.

- `<Message>` - Send a message to the user with the username `Username`.

- `/rename` - Change your `DisplayName`.

- `/join discord.Channel` - join the `Channel`.

- `/help` - Display the help message.

- `CTRL/COMMAND + C` or `CTRL/COMMAND + D` - Exit the client.

## Architecture

The client is written in C++ and uses the `unix socket` framework to communicate with the server. The client uses both UDP and TCP protocols to send and receive messages. The client is designed to be simple and easy to use. The project is OOP based and uses classes to encapsulate the functionality of the client.

Below you can see the class diagram of the client:

```mermaid
classDiagram
    class Tcp_Udp {
        +TcpUdp()
        -connectToServer()
        -sendMessageTCP(const string& message)
        -sendMessageUDP(const char* message, size_t length)
        -sendConfirm(short ID)
        -sendByeUDP(short ID)
        -handleErrFromServerUDP()
        -receiveMessageTCP()
        -receiveMessageUDP(int *err, int *bytes)
        -netrowkCommunicationThread()
        -stdinReaderThread()
        -confirmThread(short expectedID, const char* message, size_t length)
        -receiverThreadUDP()
        -receiverThreadTCP()
        -signalHandlerThread(const sigset_t& signals)
        -printMessageAsHex(const char* message, size_t length)
        -printBuffer(InputBuffer& inputBuffer)
        -setReceived(bool received)
        -mutex sendingMutex
        -mutex buffer Mutex
        -mutex confirmMutex
        -mutex confirmReceivedMutex
        -condition_variable confirmCondition
        -bool confirmReceived
        -struct sockaddr_in server_adress
        -int sock
        -unsigned short UdpTimeout
        -unsigned short UdpRetransmissions
        -string ERR_
        -enum Status
        -Status CurrentStatus
        -unsigned short SentID
        -unsigned short ReceivedID
        -string DisplayName
        -ResponseBuffer responseBuffer
        -InputBuffer inputBuffer
    }

    class Parse {
        +Parse()
        +getProtocol() const
        +getServerIP() const
        +getServerPort() const
        +getUdpTimeout() const
        +getUdpRetransmissions() const

        +parseArguments()
        +printHelp()

        -int argc
        -char** argv

        -string protocol
        -string serverIP
        -string serverPort
        -unsigned short udpTimeout
        -unsigned char udpRetransmissions
        
    }

    class ResponseBuffer {
      +addMessageToBuffer(const char* message, size_t length)
      +addMessageToBuffer(string message)
      +getMessageFromBuffer(const char* searchMessage, size_t length)
      +getMessageByIDFromBuffer(char typeID)
      +clearBuffer()
      +removeMessageFromBuffer(const char* searchMessage, size_t length)
      +printBuffer()
      +printBufferTCP()
      +isBufferEmpty()

      -mutex bufferMutex
      -vector<MessageData> messageBuffer
      -vector<string> messageBufferTCP
    }

    class InputProcess {
      +enum Type
      +string username
      +string secret
      +string displayName
      +string channel
      +string message
      +short ID

      +isValidContent(string& content)
      +isValidDName(string& dname)
      +isValidSecret(string& secret)
      +isValidID(string id)

      +parseLine(string line)
      +parseResponseAuth(string respond)
      +parseResponseMessage(string respond)
      +parseMSG(string respond)
      +parseERR(string respond)

      +toUpperCase(string& str)
      +replaceWordIgnoreCase(string text...)
      +upperTCP(string str)

      +splitString(string& str, char delimiter)
      +extractMessageContent(string& text)
      +stringToBinary(short number)
      +intToBinary(short number)

      +stringToBytes(string& s)
      +shortToBytes(short number)

      +tcp_construct_auth(Command message)
      +tcp_construct_msg(Command message)
      +tcp_construct_rename(Command message)
      +tcp_construct_join(Command message)

      +udp_construct_auth(Command message, short ID)
      +udp_construct_msg(Command message, short ID)
      +udp_construct_join(Command message, short ID)
      +udp_construct_err(short ID, string DisplayName)
      +udp_construct_bye(short ID)
      +udp_construct_confirm(short ID)

    }

    class InputBuffer {
        -std::mutex mutex
        -std::mutex network_mutex
        -std::vector<std::string> buffer
        -bool is_network_active
        +void addLine(const std::string& line)
        +std::optional<std::string> retrieveLine()
        +bool isEmpty()
        +Protocol protocol
        +const std::vector<std::string>& getBuffer() const
        +bool getNetwork()
        +void setNetwork(bool active)
        +enum Protocol
    }

    Tcp_Udp --> ResponseBuffer : gets responses from
    Tcp_Udp --> InputProcess : uses its functions
    Tcp_Udp --> InputBuffer : reads input from
```
## Testing

The project was tested by hand on reference server. Also it was tested using `netcat` for TCP protocol and `tcpdump` for UDP protocol.

*Some additional testing scripts were used provided by colleges to test the project and its edge cases.*

```
❌ 47/51 test cases passed
```
47/51 tests passed. In tests were tested such things as client state possibilities, server responses, message sending, message receiving, race conditions, allowed characters for DisplayName, Secret, Username, and Channel, etc.

The script itself runs on python3 and simulates the user behavior. So the example inputs were the same as user could provide. For example:
```
======================== ⏳ Starting test 'tcp_auth_ok' ========================
STDERR: Success: vsechno cajk
✅ Test 'tcp_auth_ok': PASSED
Test 'tcp_auth_ok' finished

Stdin: /auth a b c
```

The 4 remaining tests were failed due to unknown reasons, so they weren't fixed.

In some cases `gdb` debugging was used to find the problem in the code or the *print debugging*.

## License

This project is licensed under the GNU Affero General Public License Version 3 (AGPLv3). This license is similar to the GPL but includes an additional provision that requires you to provide source code to network users, ensuring that all users of the software, even when it's run on servers over a network, have access to its source code and the freedom to modify and share it. This makes AGPLv3 one of the most user- and freedom-protecting licenses available for open-source software.

For more details, see the [GNU official page](https://www.gnu.org/licenses/agpl-3.0.en.html) on AGPLv3.

## Acknowledgements

- **Chat GPT 4** - for helping with the project problem studying and debugging help.
- **Vitek Kvitek** - for the stream that helped with general project understanding. (repository: [IPK-Proj01-Livestream](https://github.com/okurka12/ipk_proj1_livestream))
- **Tomáš Hobza** - Unit tests for the project. (repository: [IPK-01-Tester](https://git.fit.vutbr.cz/xhobza03/ipk-client-test-server)) 
*However i used extended variant of the tester. Unfortunately i dont have link to it since I found it somewhere over discord*



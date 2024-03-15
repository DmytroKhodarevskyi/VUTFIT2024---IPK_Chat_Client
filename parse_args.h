#ifndef PARSE_H
#define PARSE_H

#include <string>
// #include <optional>

class Parse {
public:
    Parse(int argc, char** argv);

    std::string getProtocol() const;
    std::string getServerIP() const;
    unsigned short getServerPort() const;
    unsigned short getUdpTimeout() const;
    unsigned char getUdpRetransmissions() const;

    void parseArguments();
    void printHelp() const;

private:
    int argc;
    char** argv;

    // std::optional<std::string> protocol;
    std::string protocol;
    // std::optional<std::string> serverIP;
    std::string serverIP;
    unsigned short serverPort;
    unsigned short udpTimeout;
    unsigned char udpRetransmissions;
};

#endif // PARSE_H

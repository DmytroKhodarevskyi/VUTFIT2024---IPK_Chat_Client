#ifndef PARSE_H
#define PARSE_H

#include <string>

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

    std::string protocol;
    std::string serverIP;
    unsigned short serverPort;
    unsigned short udpTimeout;
    unsigned char udpRetransmissions;
};

#endif // PARSE_H

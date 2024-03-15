#include "parse_args.h"
#include <iostream>
#include <cstdlib>
#include <getopt.h>
#include <cstdlib>

Parse::Parse(int argc, char** argv) 
    // : argc(argc), argv(argv), serverPort(4567), udpTimeout(250), udpRetransmissions(3) {}
    : argc(argc), argv(argv), serverIP(""), protocol(""), serverPort(4567), udpTimeout(250), udpRetransmissions(3) {}

void Parse::parseArguments() {

    int opt;

    while ((opt = getopt(argc, argv, "t:s:p:d:r:h")) != -1) {
        switch (opt) {
            case 't':
                protocol = optarg;
                if (protocol != "tcp" && protocol != "udp") {
                    std::cerr << "Invalid protocol specified." << std::endl;
                    exit(EXIT_FAILURE);
                }
                break;
            case 's':
                serverIP = optarg;
                break;
            case 'p':
                serverPort = static_cast<unsigned short>(std::stoi(optarg));
                break;
            case 'd':
                udpTimeout = static_cast<unsigned short>(std::stoi(optarg));
                break;
            case 'r':
                udpRetransmissions = static_cast<unsigned char>(std::stoi(optarg));
                break;
            case 'h':
                printHelp();
                exit(EXIT_SUCCESS);
            default:
                printHelp();
                exit(EXIT_FAILURE);
        }
    }

    if (protocol.empty() || serverIP.empty()){
        std::cerr << "Missing required arguments." << std::endl;
        printHelp();
        exit(EXIT_FAILURE);
    }
}

std::string Parse::getProtocol() const {
    return protocol;
}

std::string Parse::getServerIP() const {
    return serverIP;
}

unsigned short Parse::getServerPort() const {
    return serverPort;
}

unsigned short Parse::getUdpTimeout() const {
    return udpTimeout;
}

unsigned char Parse::getUdpRetransmissions() const {
    return udpRetransmissions;
}

void Parse::printHelp() const {
    std::cout << "Usage: program [options]\n"
              << "-t tcp/udp\t\tTransport protocol used for connection\n"
              << "-s IP address\t\tServer IP or hostname\n"
              << "-p port\t\t\tServer port\n"
              << "-d timeout\t\tUDP confirmation timeout\n"
              << "-r retransmissions\tMaximum number of UDP retransmissions\n"
              << "-h\t\t\tPrints this help message\n";
}

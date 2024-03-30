#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For sockaddr_in
#include <arpa/inet.h> // For inet_pton
#include <unistd.h> // For close
#include <iostream>
#include <string.h> // For strlen
#include <netdb.h>

#include "parse_args.h"
#include "tcp.h"
#include "tcp_udp.h"

using namespace std;

string hostnameToIP(const string& hostname) {
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname.c_str(), nullptr, &hints, &res)) != 0) {
        cerr << "ERR: getaddrinfo: " << gai_strerror(status) << endl;
        return "";
    }

    for(p = res; p != nullptr; p = p->ai_next) {
        void *addr;
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        // Convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        break; // break after the first IP found
    }

    freeaddrinfo(res); // free the linked list
    return string(ipstr);
}


int main(int argc, char** argv) {

    Parse parse = Parse(argc, argv);
    parse.parseArguments();

    string protocol = parse.getProtocol();
    string serverIP = parse.getServerIP();
    unsigned short serverPort = parse.getServerPort();
    auto udpTimeout = parse.getUdpTimeout();
    auto udpRetransmissions = parse.getUdpRetransmissions();

    // Create a socket
    int sock;
    if (protocol == "tcp") {
        sock = socket(AF_INET, SOCK_STREAM, 0);
    }
    else if (protocol == "udp") {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (sock == -1) {
        cerr << "Could not create socket\n";
        return 1;
    }

    serverIP = hostnameToIP(serverIP);

    if (serverIP == "") {
        cerr << "ERR: Could not resolve hostname to IP\n";
        return 2;
    }

    cerr << "Protocol: " << protocol << endl;
    cerr << "Server IP: " << serverIP << endl;
    cerr << "Server Port: " << serverPort << endl;

    // Specify the server's address and port
    struct sockaddr_in server_address;

    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(serverPort); // Server port


    const char* serverIP_cstr = serverIP.c_str();
    inet_pton(AF_INET, serverIP_cstr, &server_address.sin_addr); // Server IP
    
    server_address.sin_addr.s_addr = inet_addr(serverIP.c_str());

    if (protocol == "tcp") {

        // Tcp tcp = Tcp(server_address, sock);
        TcpUdp tcp = TcpUdp(server_address, sock, udpTimeout, udpRetransmissions);
        tcp.Input(1);

    }

    else if (protocol == "udp") {
        // cerr << "UDP not implemented yet." << endl;
        // close(sock);
        // return 4;
        // cerr << "HUH?\n";
        //FIXME: Handle err
        if (bind(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            // Handle error 
        }

        TcpUdp udp = TcpUdp(server_address, sock, udpTimeout, udpRetransmissions);
        udp.Input(2);

    }

    else {
        cerr << "Invalid protocol specified." << endl;
        close(sock);
        return 5;
    }

    // // Connect to the server
    // if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
    //     cerr << "Could not connect to server\n";
    //     return 2;
    // }

    // // Send a message to the server
    // const char* message = "Hello, Server!\n\n";
    // if (send(sock, message, strlen(message), 0) == -1) {
    //     cerr << "Could not send message\n";
    //     return 3;
    // }

    // Close the socket
    close(sock);

    return 0;
}

#ifndef INPUT_PROCESS_H
#define INPUT_PROCESS_H


#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <bitset>

#include <thread>
#include <mutex>
#include <optional>

using namespace std;

class InputProcess {

public:
    typedef struct {
        enum Type {
            AUTH,
            JOIN,
            RENAME,
            HELP,
            MSG,
            NONE
    } type ;

    string username;
    string secret;
    string displayName;
    string channel;

    string message;

    short ID;

    } Command;

    bool isValidContent(std::string& content);
    bool isValidDName(const std::string& dname);
    bool isValidSecret(const std::string& secret);
    bool isValidID(std::string id);

    Command parseLine(string line);
    bool parseRespondAuth(string respond);
    bool parseRespondOpen(string respond);

    vector<string> splitString(const string& str, char delimiter);
    string stringToBinary(const std::string& s);
    string intToBinary(short number);

    vector<unsigned char> stringToBytes(const std::string& s);
    vector<unsigned char> shortToBytes(short number);





    string tcp_construct_auth(Command message);
    string tcp_construct_join(Command message);
    string tcp_construct_rename(Command message);
    string tcp_construct_msg(Command message);

    // char udp_construct_msg(Command message);
    // string udp_construct_join(Command message);
    // string udp_construct_rename(Command message);

    // string udp_construct_auth(Command message);
    vector<unsigned char> udp_construct_confirm(short ID);
    vector<unsigned char> udp_construct_auth(Command message, short ID);
    vector<unsigned char> udp_construct_bye(short ID);




};

class InputBuffer {
private:
    std::mutex mutex;
    std::vector<std::string> buffer;
    bool is_network_active;

public:
    void addLine(const std::string& line);
    std::optional<std::string> retrieveLine();
    // Example method to process and clear the buffer
    // void processAndClear();
    bool isEmpty();

    enum Protocol {
        TCP,
        UDP
    } protocol;

    const std::vector<std::string>& getBuffer() const {
        return buffer;
    }

    void setNetwork(bool active) {
        is_network_active = active;
    }

    bool getNetwork() {
        return is_network_active;
    }

};

#endif // INPUT_PROCESS_H

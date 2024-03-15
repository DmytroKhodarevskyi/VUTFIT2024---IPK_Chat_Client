#include "input_process.h"

vector<string> InputProcess::splitString(const string& str, char delimiter) {
    vector<string> tokens;
    istringstream iss(str);
    string token;
    while (getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool InputProcess::isValidContent(std::string& content) {

    content.erase(std::remove(content.begin(), content.end(), '\n'), content.end());
    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());

    if (content.empty() || content.length() > 1400) {
        return false; // Message is too long
    }

    for (char ch : content) {
        if (!((ch >= 33 && ch <= 126) || ch == 32)) {
            return false; // Character is not a VCHAR or SP
        }
    }
    return true; // All characters are valid
}

bool InputProcess::isValidDName(const std::string& dname) {

    if (dname.empty() || dname.length() > 20) {
        return false; // Message is too long
    }

    for (char ch : dname) {
        if (!((ch >= 33 && ch <= 126))) {
            return false; // Character is not a VCHAR or SP
        }
    }
    return true; // All characters are valid
}

bool InputProcess::isValidSecret(const std::string& secret) {
    // Check the length constraint first
    if (secret.empty() || secret.size() > 128) {
        return false;
    }
    
    // Check each character
    for (char ch : secret) {
        if (!(std::isalpha(ch) || std::isdigit(ch) || ch == '-')) {
            return false; // Character does not meet criteria
        }
    }
    return true; // All characters are valid
}

bool InputProcess::isValidID(std::string id) {

    string remove = "discord.";

    size_t pos = id.find(remove);

    // If the substring is found, erase it
    if (pos != std::string::npos) {
        id.erase(pos, remove.length());
    }

    // Check the length constraint first
    if (id.empty() || id.size() > 20) {
        return false;
    }
    
    // Check each character
    for (char ch : id) {
        if (!(std::isalpha(ch) || std::isdigit(ch) || ch == '-')) {
            return false; // Character does not meet criteria
        }
    }
    return true; // All characters are valid
}

bool InputProcess::parseRespondAuth(string respond) {
    vector<string> vector = splitString(respond, ' ');

    if (vector[0] == "REPLY") {
        if (vector[1] == "OK") {
            if (vector[2] == "IS") {
                if (isValidContent(vector[3])) {
                    return true;
                }
            }
        }
    }
    return false;
} 

bool InputProcess::parseRespondOpen(string respond) {
    vector<string> vector = splitString(respond, ' ');

    if (vector[0] == "REPLY") {
        if (vector[1] == "OK" || vector[1] == "NOK") {
            if (vector[2] == "IS") {
                if (isValidContent(vector[3])) {
                    return true;
                }
            }
        }
    }

    if (vector[0] == "MSG") {
        if (vector[1] == "FROM") {
            if (isValidDName(vector[2])) {
                if (vector[3] == "IS") {
                    if (isValidContent(vector[4])) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
} 

InputProcess::Command InputProcess::parseLine(string line) {

    vector<string> vector = splitString(line, ' ');

    // for (int i = 0; i < vector.size(); i++) {
    //     cout << vector[i] << endl;
    // }

    Command message = { Command::NONE, "NONE", "NONE", "NONE", "NONE", "NONE"};

    if (vector[0] == "/auth") {

        if (vector.size() != 4) {
            cerr << "ERR: " << "Invalid number of arguments for /auth" << endl;
            return message;
        }

        message.username = vector[1];
        message.secret = vector[2];
        message.displayName = vector[3];

        // if (message.secret.length() > 128) {
        if (isValidSecret(message.secret) == false) {
            cerr << "ERR: " << "Secret must be 128 characters or less" << endl;
            return message;
        }

        // if (message.username.length() > 20) {
        if (isValidID(message.username) == false){
            cerr << "ERR: " << "Username must be 20 characters or less" << endl;
            return message;
        }

        if (isValidDName(message.displayName) == false){
            cerr << "ERR: " << "Display name must be 20 characters or less" << endl;
            return message;
        }

        message.type = Command::AUTH;

    } else if (vector[0] == "/join") {

        if (vector.size() != 2) {
            cerr << "ERR: " << "Invalid number of arguments for /join" << endl;
            return message;
        }

        message.channel = vector[1];

        // if (message.channel.length() > 20) {
        if (isValidID(message.channel) == false){
            cerr << "ERR: " << "Channel must be 20 characters or less" << endl;
            return message;
        }

        message.type = Command::JOIN;
    } else if (vector[0] == "/rename") {

        if (vector.size() != 2) {
            cerr << "ERR: " << "Invalid number of arguments for /rename" << endl;
            return message;
        }

        message.type = Command::RENAME;
        message.displayName = vector[1];
    } else if (vector[0] == "/help") {

        message.type = Command::HELP;
        std::cerr 
              << "Usage:\n"
              << "\n"
              << "/auth {username} {secret} {display_name}\n"
              << "Sends AUTH message with the data provided from the command to the server\n"

              << "(and correctly handles the Reply message), locally sets the DisplayName value\n" 
              << "(same as the /rename command)\n"
              << "\n"
              << "/join {channel}\n"
              << "Sends JOIN message with the data provided from the command to the server\n" 
              << "(and correctly handles the Reply message)\n"
              << "\n"
              << "/rename {display_name}\n"
              << "Sends RENAME message with the data provided from the command to the server\n" 
              << "(and correctly handles the Reply message), locally sets the DisplayName value\n"
              << "\n"
              << "/help\n"
              << "Prints this help message\n"
              << "\n"
              << "CTRL + D\n"
              << "Exits the program";

              return message;
    } else {
        message.type = Command::MSG;
        message.message= line;

        return message;
    }

    return message;
}

string InputProcess::tcp_construct_auth(Command message) {
    string auth = "AUTH " + message.username + " AS " + message.displayName + " USING " + message.secret + "\r\n";
    return auth;
}

string InputProcess::tcp_construct_join(Command message) {
    string join = "JOIN " + message.channel + " AS " + message.displayName + "\r\n";
    return join;
}

string InputProcess::tcp_construct_msg(Command message) {
    string msg = "MSG FROM " + message.displayName + " IS " + message.message + "\r\n";
    return msg;
}


void InputBuffer::addLine(const std::string& line) {
        std::lock_guard<std::mutex> guard(mutex);
        buffer.push_back(line);
}

// void InputBuffer::processAndClear() {
//         std::lock_guard<std::mutex> guard(mutex);
//         for (const auto& line : buffer) {
//             // Process each line here
//             // std::cout << "Processing buffered line: " << line << std::endl;
//         }
//         buffer.clear();
// }

std::optional<std::string> InputBuffer::retrieveLine() {
        std::lock_guard<std::mutex> guard(mutex);
        if (!buffer.empty()) {
            std::string line = buffer.front(); // Get the first line
            buffer.erase(buffer.begin()); // Remove the first line from the buffer
            return line;
        }
        return {}; // Return an empty optional if the buffer is empty
}


bool InputBuffer::isEmpty() {
        std::lock_guard<std::mutex> guard(mutex);
        return buffer.empty();
}
#include "response_buffer.h"

void ResponseBuffer::addMessageToBuffer(string message) {
    lock_guard<mutex> lock(bufferMutex); // Lock the buffer for thread safety

    messageBufferTCP.push_back(message); // Add the message to the vector
}

void ResponseBuffer::printBufferTCP() const {
    cerr << "----------------------" << endl; // Print a separator line
    cerr << "Buffer contents:" << endl;
    for (const auto& message : messageBufferTCP) {
        cerr << message << endl;
    }
    cerr << "----------------------" << endl; // Print a separator line
}

string ResponseBuffer::getMessageFromBufferTCP(const string& type) {
    lock_guard<mutex> lock(bufferMutex); // Lock the buffer for thread safety

    for (auto it = messageBufferTCP.begin(); it != messageBufferTCP.end(); ++it) {
        istringstream iss(*it);
        string firstWord;
        iss >> firstWord; // Extract the first word from the message

        if (firstWord == type) {
            string message = *it; // Copy the matching message
            messageBufferTCP.erase(it); // Remove the message from the buffer
            return message; // Return the entire message that starts with the given type (word)
        }
    }

    return ""; // Return an empty string if no matching message is found
}

void ResponseBuffer::addMessageToBuffer(const char* message, size_t length) {
    MessageData msgData;
    msgData.message = message; // Store the pointer directly
    msgData.length = length;

    messageBuffer.push_back(msgData); // Add the structured message to the vector
}

const char* ResponseBuffer::getMessageFromBuffer(const char* searchMessage, size_t length) {
        lock_guard<mutex> lock(bufferMutex); // Lock the buffer
        for (auto& msgData : messageBuffer) {

            if (msgData.length == length && std::memcmp(msgData.message, searchMessage, length) == 0) {
                return msgData.message;
            }
        }
        return nullptr; // Message not found
    }

tuple <const char*, size_t> ResponseBuffer::getMessageByIDFromBuffer(char typeID) {
    lock_guard<mutex> lock(bufferMutex); // Lock the buffer

    // printBuffer();
    for (int i = 0; i < 3; i++) {
        for (auto& msgData : messageBuffer) {

            if (*msgData.message == typeID) {
                return make_tuple(msgData.message, msgData.length);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return make_tuple(nullptr, 0); // Message with specified type ID not found
}

bool ResponseBuffer::removeMessageFromBuffer(const char* searchMessage, size_t length) {
    lock_guard<mutex> lock(bufferMutex); // Lock the buffer
    bool found = false;

    for (auto it = messageBuffer.begin(); it != messageBuffer.end(); /* no increment here */) {
        if (it->length == length && std::memcmp(it->message, searchMessage, length) == 0) {
            free(const_cast<char*>(it->message)); // Free the memory
            it = messageBuffer.erase(it); // Remove from the buffer and get the next iterator
            found = true; // Mark as found
        } else {
            ++it; // Only increment if not erasing
        }
    }

    return found; // Return true if at least one message was found and removed
}


void ResponseBuffer::clearBuffer() {
    lock_guard<mutex> lock(bufferMutex); // Lock the buffer for thread safety

    for (auto& msgData : messageBuffer) {
        free(const_cast<char*>(msgData.message));
    }

    messageBuffer.clear();

}



  void ResponseBuffer:: printBuffer() const {
        cerr << "----------------------\n";
        cerr << "Buffer contents:\n";
        for (const auto& msgData : messageBuffer) {
            cerr << "Message: ";
            for (size_t i = 0; i < msgData.length; ++i) {
                cerr << std::hex << std::setw(2) << std::setfill('0')
                          << (0xFF & static_cast<int>(msgData.message[i])) << " ";
            }
            cerr << std::dec << "\n";
        }
        cerr << "----------------------\n";
    }

    bool ResponseBuffer::isBufferEmpty() const {
        return messageBuffer.empty();
    }
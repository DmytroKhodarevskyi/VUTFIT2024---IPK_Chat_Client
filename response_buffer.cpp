#include "response_buffer.h"

// void ResponseBuffer::addMessageToBuffer(MemoryManager &memoryManager, const char* message, size_t length) {
//         // char* buffer = new char[length];
//         char* buffer = memoryManager.allocate<char>(length);
//         std::memcpy(buffer, message, length); // Copy the message to the new buffer
//         messageBuffer.push_back(buffer); // Add the buffer to the vector
// }

void ResponseBuffer::addMessageToBuffer(string message) {
    lock_guard<mutex> lock(bufferMutex); // Lock the buffer for thread safety

    messageBufferTCP.push_back(message); // Add the message to the vector
}

void ResponseBuffer::printBufferTCP() const {
    // lock_guard<mutex> lock(bufferMutex); // Ensure thread-safe access to the buffer

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

// char* ResponseBuffer::getMessageFromBuffer(const char* searchMessage, size_t length) {
//     for (char* buffer : messageBuffer) {
//         if (std::memcmp(buffer, searchMessage, length) == 0) {
//             return buffer;
//         }
//     }
//     return nullptr; // Message not found
// }

const char* ResponseBuffer::getMessageFromBuffer(const char* searchMessage, size_t length) {
        lock_guard<mutex> lock(bufferMutex); // Lock the buffer
        for (auto& msgData : messageBuffer) {

            // msgData.message = const_cast<char*>(msgData.message);

            // cerr << "msgData.message: " << msgData.message << endl;
            // cerr << "searchMessage: " << searchMessage << endl;
            // cerr << "length: " << length << endl;

            // for(auto byte : msgData.message) {
            //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
            // }


            if (msgData.length == length && std::memcmp(msgData.message, searchMessage, length) == 0) {
                return msgData.message;
            }
        }
        // cerr << "Message not found :((((((((((((\n";
        return nullptr; // Message not found
    }

tuple <const char*, size_t> ResponseBuffer::getMessageByIDFromBuffer(char typeID) {
    lock_guard<mutex> lock(bufferMutex); // Lock the buffer

    // printBuffer();
    for (int i = 0; i < 3; i++) {
        for (auto& msgData : messageBuffer) {
            // Logging for debugging purposes
            // cerr << "msgData.message (type ID): " << static_cast<int>(*msgData.message) << endl;
            // cerr << "searched type ID: " << static_cast<int>(typeID) << endl;

            // Compare only the first byte of the message to the typeID
            if (*msgData.message == typeID) {
                return make_tuple(msgData.message, msgData.length);
            }
        }
        // std::chrono::milliseconds dura(1000);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // cerr << "Message with specified ID not found :((((((((((((\n";
    return make_tuple(nullptr, 0); // Message with specified type ID not found
}
// bool ResponseBuffer::removeMessageFromBuffer(MemoryManager &memoryManager, const char* searchMessage, size_t length) {
//     for (auto it = messageBuffer.begin(); it != messageBuffer.end(); ++it) {
//         if (std::memcmp(*it, searchMessage, length) == 0) {
//             // delete[] *it; // Free the memory
//             memoryManager.freeSpecific(*it);
//             messageBuffer.erase(it); // Remove from the buffer
//             return true; // Message was found and removed
//         }
//     }
//     return false; // Message not found
// }

//   bool ResponseBuffer:: removeMessageFromBuffer(MemoryManager &memoryManager, const char* searchMessage, size_t length) {
// bool ResponseBuffer:: removeMessageFromBuffer(const char* searchMessage, size_t length) {
//     lock_guard<mutex> lock(bufferMutex); // Lock the buffer
//     for (auto it = messageBuffer.begin(); it != messageBuffer.end(); ++it) {
//         if (it->length == length && std::memcmp(it->message, searchMessage, length) == 0) {
//             // memoryManager.freeSpecific(it->message);
//             free(const_cast<char*>(it->message));
//             messageBuffer.erase(it); // Remove from the buffer
//             return true; // Message was found and removed
//         }
//     }
//     return false; // Message not found
// }

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
        // Free the memory allocated for each message
        // Assuming the memory for messages was allocated with 'new' or 'malloc' and needs to be freed.
        free(const_cast<char*>(msgData.message));
    }

    // Clear the vector after freeing the memory to avoid dangling pointers
    messageBuffer.clear();

    // cout << "Buffer cleared.\n";
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
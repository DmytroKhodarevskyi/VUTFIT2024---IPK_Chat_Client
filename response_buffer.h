#include <vector>
#include <cstring> // For std::memcmp
#include <iostream>
#include <iomanip>
#include "memory_manager.h"
#include <mutex>

using namespace std;

class ResponseBuffer {
public:
    // ... (other class members and methods)
    // void addMessageToBuffer(MemoryManager &memoryManager, const char* message, size_t length);
    void addMessageToBuffer(const char* message, size_t length);
   
    // Function to search for and retrieve a message from the buffer
    // char* getMessageFromBuffer(const char* searchMessage, size_t length);
    const char* getMessageFromBuffer(const char* searchMessage, size_t length);
    
    const char* getMessageByIDFromBuffer(char typeID);

    void clearBuffer();
    // Function to remove a message from the buffer
    // bool removeMessageFromBuffer(const char* searchMessage, size_t length);
    //  bool removeMessageFromBuffer(MemoryManager &memoryManager, const char* searchMessage, size_t length);
     bool removeMessageFromBuffer(const char* searchMessage, size_t length);
    
    void printBuffer() const;


private:
    mutex bufferMutex; // Mutex to protect the buffer
    // std::vector<char*> messageBuffer; // Buffer of pointers to byte messages
    typedef struct {
        const char* message;
        size_t length;
    } MessageData;

    std::vector<MessageData> messageBuffer; // Buffer of pointers to byte messages
    // ... (other private members)
};

#include <vector>
#include <cstring> // For std::memcmp
#include <iostream>
#include <iomanip>
#include <mutex>

#include <tuple>

#include <thread>
#include <chrono>

using namespace std;

class ResponseBuffer {
public:
    void addMessageToBuffer(const char* message, size_t length);
    void addMessageToBuffer(string message);

    string getMessageFromBufferTCP(const string& type);
   
    const char* getMessageFromBuffer(const char* searchMessage, size_t length);
    
    tuple <const char*, size_t> getMessageByIDFromBuffer(char typeID);

    void clearBuffer();
    bool removeMessageFromBuffer(const char* searchMessage, size_t length);
    
    void printBuffer() const;
    
    void printBufferTCP() const;


    bool isBufferEmpty() const;


private:
    mutex bufferMutex; // Mutex to protect the buffer
    typedef struct {
        const char* message;
        size_t length;
    } MessageData;

    std::vector<MessageData> messageBuffer; // Buffer of pointers to byte messages
    
    std::vector<string> messageBufferTCP;
};

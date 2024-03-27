#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <vector>
#include <cstdlib> // For std::malloc and std::free
#include <algorithm> // For std::find and std::remove

class MemoryManager {
public:
    ~MemoryManager();
    template <typename T>
    T* allocate(size_t numElements = 1);
    template <typename T>
    void freeSpecific(T* ptr);
    void freeAll();

private:
    std::vector<void*> allocations;
};

#include "memory_manager.tpp"

#endif // MEMORYMANAGER_H

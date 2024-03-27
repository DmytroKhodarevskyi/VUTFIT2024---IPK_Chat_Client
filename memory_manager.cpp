#include "memory_manager.h"

MemoryManager::~MemoryManager() {
    freeAll();
}

void MemoryManager::freeAll() {
    for (void* ptr : allocations) {
        std::free(ptr);
    }
    allocations.clear();
}

// Template method definitions for MemoryManager

template <typename T>
T* MemoryManager::allocate(size_t numElements) {
    T* ptr = static_cast<T*>(std::malloc(numElements * sizeof(T)));
    if (ptr) {
        allocations.push_back(reinterpret_cast<void*>(ptr));
    } else {
        // Handle allocation failure; throw an exception or return nullptr
    }
    return ptr;
}

template <typename T>
void MemoryManager::freeSpecific(T* ptr) {
    auto it = std::find(allocations.begin(), allocations.end(), reinterpret_cast<void*>(const_cast<char*>(ptr)));
    if (it != allocations.end()) {
        // std::free(reinterpret_cast<void*>(ptr));
        std::free(reinterpret_cast<void*>(const_cast<char*>(ptr)));

        allocations.erase(it);
    }
}

// Destructor and freeAll are not templated and can be defined in a .cpp file

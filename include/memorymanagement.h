#ifndef __MYOS__MEMORYMANAGEMENT_H
#define __MYOS__MEMORYMANAGEMENT_H

#include <common/types.h>

namespace myos
{
    // The MemoryChunk structure represents a block of memory in a linked list.
    // Each chunk has pointers to the next and previous chunks, a flag indicating
    // whether it is currently allocated, and its size.
    struct MemoryChunk
    {
        MemoryChunk *next;     // Pointer to the next memory chunk in the list
        MemoryChunk *prev;     // Pointer to the previous memory chunk in the list
        bool allocated;        // Flag to indicate if this chunk is currently allocated
        common::size_t size;   // Size of this memory chunk
    };
    
    // The MemoryManager class is responsible for managing dynamic memory allocation
    // within the operating system. It provides mechanisms to allocate (`malloc`) 
    // and free (`free`) memory, managing the memory in a linked list of chunks.
    class MemoryManager
    {
    protected:
        MemoryChunk* first;    // Pointer to the first memory chunk in the linked list

    public:
        static MemoryManager *activeMemoryManager; // Pointer to the active memory manager instance

        // Constructor: Initializes the memory manager with the given memory range.
        // `first` is the starting address of the memory, and `size` is the total size of the memory pool.
        MemoryManager(common::size_t first, common::size_t size);

        // Destructor: Cleans up resources used by the memory manager.
        ~MemoryManager();
        
        // Allocates a block of memory of the requested size.
        // Returns a pointer to the allocated memory or nullptr if allocation fails.
        void* malloc(common::size_t size);

        // Frees a previously allocated block of memory, making it available for future allocations.
        void free(void* ptr);
    };
}

// Overloaded global `new` operator to allocate memory using the custom memory manager.
void* operator new(unsigned size);
void* operator new[](unsigned size);

// Placement new operator: Constructs an object at a specific memory location (provided by `ptr`).
void* operator new(unsigned size, void* ptr);
void* operator new[](unsigned size, void* ptr);

// Overloaded global `delete` operator to free memory using the custom memory manager.
void operator delete(void* ptr);
void operator delete[](void* ptr);

#endif

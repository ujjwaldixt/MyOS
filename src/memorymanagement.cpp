#include <memorymanagement.h>

/*
 * We place our code in the "myos" namespace, which provides an operating system context.
 * The "myos::common" namespace often holds basic type aliases (e.g., size_t).
 */
using namespace myos;
using namespace myos::common;


/*
 * ----------------------------------------------------------------------------
 * MemoryManager Class
 * ----------------------------------------------------------------------------
 *
 * Manages heap memory allocations and deallocations in a simple linked-list fashion.
 * "MemoryChunk" is the fundamental unit that tracks whether a memory block is allocated,
 * its size, and links to adjacent chunks.
 *
 * The memory layout is:
 *  [ MemoryChunk (header) ] [ user data bytes ] [ next MemoryChunk (header) ] [ ... ]
 *
 * Each MemoryChunk is placed in the same region it describes, 
 * so overhead must be considered when creating or splitting chunks.
 */

/*
 * activeMemoryManager:
 *  A static pointer to the global, currently active memory manager.
 *  "new" and "delete" operators rely on this global pointer to allocate or free memory.
 */
MemoryManager* MemoryManager::activeMemoryManager = 0;


/*
 * Constructor:
 *   - start: the starting address of the memory region for heap allocation.
 *   - size: the total size (in bytes) of that region.
 * It sets up the first MemoryChunk to describe this entire region, 
 * marking it as free (allocated=false).
 */
MemoryManager::MemoryManager(size_t start, size_t size)
{
    // The newly created manager becomes the active one
    activeMemoryManager = this;
    
    // If there isn't enough space to store at least a MemoryChunk, 
    // we can't create any chunk at all.
    if(size < sizeof(MemoryChunk))
    {
        first = 0;
    }
    else
    {
        // Place the first chunk at 'start' with the entire region
        first = (MemoryChunk*)start;
        
        first->allocated = false;
        first->prev = 0;
        first->next = 0;
        // The chunk size is total region size minus the header size
        first->size = size - sizeof(MemoryChunk);
    }
}

/*
 * Destructor:
 *  - If this manager is still the activeMemoryManager, reset the global pointer to 0.
 */
MemoryManager::~MemoryManager()
{
    if(activeMemoryManager == this)
        activeMemoryManager = 0;
}


/*
 * malloc:
 *  - Allocates a block of memory of at least 'size' bytes.
 *  - Scans through the linked list of MemoryChunks to find the first free chunk 
 *    large enough to accommodate 'size'.
 *  - If the chunk is larger than necessary, it's split into two chunks:
 *      one of exactly 'size' bytes (allocated = true)
 *      and a second chunk for the remainder.
 *  - Returns a pointer to the usable memory (just after the MemoryChunk header).
 */
void* MemoryManager::malloc(size_t size)
{
    MemoryChunk* result = 0;
    
    // Find a free chunk with enough space
    for(MemoryChunk* chunk = first; chunk != 0 && result == 0; chunk = chunk->next)
        if(chunk->size > size && !chunk->allocated)
            result = chunk;
        
    // No suitable chunk found
    if(result == 0)
        return 0;
    
    // If chunk is big enough to split into allocated + free remainder
    if(result->size >= size + sizeof(MemoryChunk) + 1)
    {
        // Create a new chunk after the allocated block
        MemoryChunk* temp = (MemoryChunk*)((size_t)result + sizeof(MemoryChunk) + size);
        
        temp->allocated = false;
        temp->size = result->size - size - sizeof(MemoryChunk);
        temp->prev = result;
        temp->next = result->next;
        if(temp->next != 0)
            temp->next->prev = temp;
        
        result->size = size;
        result->next = temp;
    }
    
    // Mark the chosen chunk as allocated
    result->allocated = true;
    // Return the address after the chunk header
    return (void*)(((size_t)result) + sizeof(MemoryChunk));
}

/*
 * free:
 *  - Frees a previously allocated block of memory pointed to by 'ptr'.
 *  - Merges (coalesces) adjacent free chunks if possible.
 */
void MemoryManager::free(void* ptr)
{
    // The chunk header is located immediately before the pointer
    MemoryChunk* chunk = (MemoryChunk*)((size_t)ptr - sizeof(MemoryChunk));
    
    chunk->allocated = false;
    
    // Merge with previous chunk if it's free
    if(chunk->prev != 0 && !chunk->prev->allocated)
    {
        chunk->prev->next = chunk->next;
        chunk->prev->size += chunk->size + sizeof(MemoryChunk);
        if(chunk->next != 0)
            chunk->next->prev = chunk->prev;
        
        chunk = chunk->prev;
    }
    
    // Merge with next chunk if it's free
    if(chunk->next != 0 && !chunk->next->allocated)
    {
        chunk->size += chunk->next->size + sizeof(MemoryChunk);
        chunk->next = chunk->next->next;
        if(chunk->next != 0)
            chunk->next->prev = chunk;
    }
}


/*
 * Overloaded new and delete operators:
 *  - The C++ global "new" and "delete" operators are linked to our MemoryManager,
 *    allowing normal usage of "new" / "delete" in kernel code.
 */

/*
 * operator new(unsigned size):
 *   - Uses the active memory manager (if any) to allocate 'size' bytes.
 *   - Returns 0 if there's no active manager or if allocation fails.
 */
void* operator new(unsigned size)
{
    if(myos::MemoryManager::activeMemoryManager == 0)
        return 0;
    return myos::MemoryManager::activeMemoryManager->malloc(size);
}

/*
 * operator new[](unsigned size):
 *   - Same as operator new, but for array allocations (technically the same logic).
 */
void* operator new[](unsigned size)
{
    if(myos::MemoryManager::activeMemoryManager == 0)
        return 0;
    return myos::MemoryManager::activeMemoryManager->malloc(size);
}

/*
 * Placement new operators:
 *   - These versions of new do not allocate memory but construct an object in 
 *     an already provided memory location 'ptr'.
 */
void* operator new(unsigned size, void* ptr)
{
    return ptr;
}

void* operator new[](unsigned size, void* ptr)
{
    return ptr;
}

/*
 * operator delete:
 *   - Frees memory using the active memory manager (if set).
 */
void operator delete(void* ptr)
{
    if(myos::MemoryManager::activeMemoryManager != 0)
        myos::MemoryManager::activeMemoryManager->free(ptr);
}

/*
 * operator delete[]:
 *   - Same as operator delete, but for array deallocations.
 */
void operator delete[](void* ptr)
{
    if(myos::MemoryManager::activeMemoryManager != 0)
        myos::MemoryManager::activeMemoryManager->free(ptr);
}

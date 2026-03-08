#include "memorymanager.h"
#include "log.h"

LPVOID MemoryManager::ReserveAddressSpace(uintptr_t minAddr, uintptr_t maxAddr, uintptr_t &reservedBase, uintptr_t &reservedSize)
{
    // Windows VirtualAlloc MEM_RESERVE requires 64KB alignment (0xFFFF mask)
    reservedBase = minAddr & ~0xFFFF;
    // Calculate size covering from aligned base to (maxAddr aligned up)
    uintptr_t alignedMax = (maxAddr + 0xFFFF) & ~0xFFFF;
    reservedSize = alignedMax - reservedBase;

    LPVOID result = VirtualAlloc((LPVOID)reservedBase, reservedSize, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (result != NULL)
    {
        reservedBase = (uintptr_t)result;
        // log_info("MemoryManager: Reserved %u MB at 0x%p", reservedSize / 1024 / 1024, result);
    }
    else 
    {
        log_error("MemoryManager: VirtualAlloc MEM_RESERVE failed! Base: 0x%p, Size: 0x%x (%u MB), Error: %lu", 
            (void*)reservedBase, reservedSize, reservedSize / 1024 / 1024, GetLastError());
    }

    return result;
}

bool MemoryManager::CommitSegment(uintptr_t vaddr, uintptr_t memsz)
{
    // VirtualAlloc MEM_COMMIT requires 4KB alignment? Actually it rounds down automatically, 
    // but the size must be enough to cover the range.
    
    // Page align down
    uintptr_t alignedVAddr = vaddr & ~0xFFF;
    
    // Calculate end address
    uintptr_t vEnd = vaddr + memsz;
    
    // Page align up end address
    uintptr_t alignedEnd = (vEnd + 0xFFF) & ~0xFFF;
    
    // Size is diff
    uintptr_t alignedSize = alignedEnd - alignedVAddr;

    LPVOID result = VirtualAlloc((LPVOID)alignedVAddr, alignedSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    return result != NULL;
}
#include "memorymanager.h"

LPVOID MemoryManager::ReserveAddressSpace(uintptr_t minAddr, uintptr_t maxAddr, uintptr_t& reservedBase, uintptr_t& reservedSize) {
    reservedBase = minAddr & ~0xFFFF;
    reservedSize = (maxAddr - reservedBase + 0xFFFF) & ~0xFFFF;
    return VirtualAlloc((LPVOID)reservedBase, reservedSize, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

bool MemoryManager::CommitSegment(uintptr_t vaddr, uintptr_t memsz) {
    uintptr_t alignedVAddr = vaddr & ~0xFFF;
    uintptr_t offsetInPage = vaddr & 0xFFF;
    uintptr_t alignedSize = (memsz + offsetInPage + 0xFFF) & ~0xFFF;
    return VirtualAlloc((LPVOID)alignedVAddr, alignedSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE) != NULL;
}
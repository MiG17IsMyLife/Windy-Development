#include "linuxstack.h"

uint32_t LinuxStack::Setup(uint32_t size, const char* argv0) {
    void* stack = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint32_t esp = (uint32_t)stack + size - 64;
    uint32_t* ptr = (uint32_t*)esp;

    *(--ptr) = 0; // envp
    *(--ptr) = 0; // argv end
    *(--ptr) = (uint32_t)argv0;
    *(--ptr) = 1; // argc
    return (uint32_t)ptr;
}
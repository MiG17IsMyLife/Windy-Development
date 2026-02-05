#define _CRT_SECURE_NO_WARNINGS
#include "linuxstack.h"

uint32_t LinuxStack::Setup(uint32_t size, const std::vector<std::string>& args) {
    void* stack = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint32_t esp = (uint32_t)stack + size - 64;
    uint32_t* ptr = (uint32_t*)esp;

    // We need to copy strings to the stack first? 
    // Usually strings are stored at the top of the stack (higher addresses)
    // and pointers to them are stored below.
    // For simplicity, let's put strings right below the initial ESP for now, 
    // or allocate them separately? 
    // Actually, usually they are at the very top.
    
    // Let's copy strings to the very end of the allocated stack 
    // and move backwards.
    
    char* stringArea = (char*)((uint32_t)stack + size);
    std::vector<uint32_t> argvPointers;

    for (const auto& arg : args) {
        size_t len = arg.length() + 1;
        stringArea -= len;
        strcpy(stringArea, arg.c_str());
        argvPointers.push_back((uint32_t)stringArea);
    }

    // Align ESP
    ptr = (uint32_t*)((uint32_t)stringArea & ~0xF); // 16-byte align
    ptr -= 4; // Space for something?

    // Now push pointers
    *(--ptr) = 0; // envp (NULL)
    *(--ptr) = 0; // null terminator for argv

    // Push argv pointers in reverse order
    for (auto it = argvPointers.rbegin(); it != argvPointers.rend(); ++it) {
        *(--ptr) = *it;
    }
    
    // Linux _start Calling Convention:
    // ESP -> argc
    // ESP+4 -> argv[0]
    // ...
    // ESP+4*(argc+1) -> NULL
    
    *(--ptr) = args.size(); // argc

    return (uint32_t)ptr;
}
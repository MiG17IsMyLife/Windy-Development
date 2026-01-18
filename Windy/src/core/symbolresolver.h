#pragma once
#include <stdint.h>

class SymbolResolver {
public:
    static void ResolveAll(uint32_t jmpRel, uint32_t symTab, uint32_t strTab, uint32_t pltRelSize);
private:
    static uintptr_t GetExternalAddr(const char* name);
};
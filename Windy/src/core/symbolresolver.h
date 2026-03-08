#pragma once
#include <stdint.h>

class SymbolResolver {
public:
    static void ResolveAll(uintptr_t jmpRel, uintptr_t relTab, uintptr_t symTab, uintptr_t strTab, uint32_t pltRelSize, uint32_t relSize, uint32_t relEnt, uintptr_t baseAddr = 0);
private:
    static uintptr_t GetExternalAddr(const char* name);
};
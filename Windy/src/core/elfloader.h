#pragma once

#include <string>
#include <vector>
#include "common.h"

class ElfLoader {
private:
    std::string filePath;
    Elf32_Ehdr ehdr;
    uintptr_t jmpRel;
    uintptr_t symTab;
    uintptr_t strTab;
    uint32_t pltRelSize;

public:
    ElfLoader(const char* path);
    ~ElfLoader();

    bool LoadToMemory();

    const Elf32_Ehdr& GetHeader() const { return ehdr; }
    uintptr_t GetJmpRel() const { return jmpRel; }
    uintptr_t GetSymTab() const { return symTab; }
    uintptr_t GetStrTab() const { return strTab; }
    uint32_t GetPltRelSize() const { return pltRelSize; }
};
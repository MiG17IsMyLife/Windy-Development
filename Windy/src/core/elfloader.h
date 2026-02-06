#pragma once

#include <string>
#include <vector>
#include "common.h"
#include "ilibraryloader.h"

class ElfLoader : public ILibraryLoader {
private:
    std::string filePath;
    Elf32_Ehdr ehdr;
    uintptr_t jmpRel;
    uintptr_t symTab;
    uint32_t symCount; // Fallback for simple iteration
    uintptr_t strTab;
    uint32_t pltRelSize;

    // Relocation Tables (DT_REL)
    uintptr_t rel;
    uint32_t relSz;
    uint32_t relEnt;

    // Packed Relocations (DT_RELR)
    uintptr_t relr;
    uintptr_t relrSz;
    uintptr_t relrEnt;

    // Symbol Hash Table (DT_HASH)
    uintptr_t hash;
    // GNU Hash Table (DT_GNU_HASH)
    uintptr_t gnuHash;

    // Initialization (DT_INIT / DT_INIT_ARRAY)
    uintptr_t initFunc;
    uintptr_t initArray;
    uint32_t initArraySz;

    // New fields for Multi-ELF support

    // New fields for Multi-ELF support
    uintptr_t loadedBase;
    size_t loadedSize;
    std::vector<std::string> dependencies;

public:
    ElfLoader(const char* path);
    ~ElfLoader();

    // Load with optional manual base address (for shared objects)
    bool LoadToMemory(uintptr_t manualBase = 0);
    
    // Perform relocations (R_386_RELATIVE) after loading
    bool Relocate(uintptr_t baseAddr);
    
    // Run Initialization (DT_INIT / DT_INIT_ARRAY)
    void RunInit() override;
    
    // Debug helper
    void DebugPrintSymbols() const override;

    const Elf32_Ehdr& GetHeader() const { return ehdr; }
    uintptr_t GetJmpRel() const { return jmpRel; }
    uintptr_t GetSymTab() const { return symTab; }
    uintptr_t GetStrTab() const { return strTab; }
    uint32_t GetPltRelSize() const { return pltRelSize; }

    // Generic Symbol Lookup (Exported)
    uintptr_t GetSymbolAddress(const char* name) const override;

    // Accessors for dependency walking
    const std::vector<std::string>& GetDependencies() const override { return dependencies; }
    uintptr_t GetLoadedBase() const { return loadedBase; }
    size_t GetLoadedSize() const { return loadedSize; }
};
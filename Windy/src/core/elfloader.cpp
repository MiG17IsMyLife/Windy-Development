#include "elfloader.h"
#include "memorymanager.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>

ElfLoader::ElfLoader(const char *path) : filePath(path), jmpRel(0), symTab(0), strTab(0), pltRelSize(0)
{
    memset(&ehdr, 0, sizeof(ehdr));
}

ElfLoader::~ElfLoader()
{
}

bool ElfLoader::LoadToMemory()
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to open ELF file: " << filePath << std::endl;
        return false;
    }

    file.read((char *)&ehdr, sizeof(ehdr));
    if (file.gcount() != sizeof(ehdr))
        return false;

    // ELFMAG (0x7F 'E' 'L' 'F') check
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0)
    {
        std::cerr << "Invalid ELF header." << std::endl;
        return false;
    }

    // Min and Max address calculation
    uintptr_t minAddr = 0xFFFFFFFF;
    uintptr_t maxAddr = 0;

    // Read Program Headers
    std::vector<Elf32_Phdr> phdrs(ehdr.e_phnum);
    file.seekg(ehdr.e_phoff);
    file.read((char *)phdrs.data(), sizeof(Elf32_Phdr) * ehdr.e_phnum);

    for (const auto &phdr : phdrs)
    {
        if (phdr.p_type == PT_LOAD)
        {
            if (phdr.p_vaddr < minAddr)
                minAddr = phdr.p_vaddr;
            if (phdr.p_vaddr + phdr.p_memsz > maxAddr)
                maxAddr = phdr.p_vaddr + phdr.p_memsz;
        }
    }

    uintptr_t base, size;
    // Reserve address space
    if (!MemoryManager::ReserveAddressSpace(minAddr, maxAddr, base, size))
    {
        std::cerr << "Failed to reserve memory." << std::endl;
        return false;
    }

    for (const auto &phdr : phdrs)
    {
        if (phdr.p_type == PT_LOAD)
        {
            // Commit segment
            MemoryManager::CommitSegment(phdr.p_vaddr, phdr.p_memsz);

            // Read data from file
            if (phdr.p_filesz > 0)
            {
                file.seekg(phdr.p_offset);
                file.read((char *)phdr.p_vaddr, phdr.p_filesz);
            }

            // Zero-clear BSS region
            if (phdr.p_memsz > phdr.p_filesz)
            {
                memset((char *)phdr.p_vaddr + phdr.p_filesz, 0, phdr.p_memsz - phdr.p_filesz);
            }
        }
        if (phdr.p_type == PT_DYNAMIC)
        {
            // Dynamic section parsing
            size_t count = phdr.p_filesz / sizeof(Elf32_Dyn);
            std::vector<Elf32_Dyn> dyns(count);

            file.seekg(phdr.p_offset);
            file.read((char *)dyns.data(), phdr.p_filesz);

            for (const auto &d : dyns)
            {
                if (d.d_tag == DT_SYMTAB)
                    symTab = d.d_un.d_ptr;
                if (d.d_tag == DT_STRTAB)
                    strTab = d.d_un.d_ptr;
                if (d.d_tag == DT_JMPREL)
                    jmpRel = d.d_un.d_ptr;
                if (d.d_tag == DT_PLTRELSZ)
                    pltRelSize = d.d_un.d_val;
            }
        }
    }
    return true;
}
#include "elfloader.h"
#include "memorymanager.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>
#include "log.h"
#include "symbolresolver.h"

// Re-defining here just in case common.h update isn't fully propagated in IDE context, 
// ensuring standalone correctness.
#ifndef DT_HASH
#define DT_HASH     4
#endif
#ifndef DT_INIT
#define DT_INIT     12
#endif
#ifndef DT_INIT_ARRAY
#define DT_INIT_ARRAY 25
#endif
#ifndef DT_INIT_ARRAYSZ
#define DT_INIT_ARRAYSZ 27
#endif
#ifndef R_386_RELATIVE
#define R_386_RELATIVE 8
#endif

#ifndef ELF32_R_TYPE
#define ELF32_R_TYPE(val) ((val) & 0xff)
#endif
#ifndef ELF32_ST_BIND
#define ELF32_ST_BIND(i) ((i) >> 4)
#endif

// Define SHT_DYNSYM if missing
#ifndef SHT_DYNSYM
#define SHT_DYNSYM 11
#endif

// ELF Hash Function
static unsigned long elf_hash(const unsigned char *name) {
    unsigned long h = 0, g;
    while (*name) {
        h = (h << 4) + *name++;
        if ((g = h & 0xf0000000))
            h ^= g >> 24;
        h &= ~0xf0000000;
    }
    return h;
}

// GNU Hash Function
static uint32_t dl_new_hash(const char *s) {
    uint32_t h = 5381;
    for (unsigned char c = *s; c != '\0'; c = *++s)
        h = h * 33 + c;
    return h;
}

ElfLoader::ElfLoader(const char* path)
    : filePath(path), jmpRel(0), symTab(0), strTab(0), pltRelSize(0), 
      rel(0), relSz(0), relEnt(0), 
      relr(0), relrSz(0), relrEnt(0),
      hash(0), gnuHash(0), initFunc(0), initArray(0), initArraySz(0),
      symCount(0),
      loadedBase(0), loadedSize(0) {
    memset(&ehdr, 0, sizeof(ehdr));
}

ElfLoader::~ElfLoader() {}

bool ElfLoader::LoadToMemory(uintptr_t manualBase) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open ELF file: " << filePath << std::endl;
        return false;
    }

    file.read((char*)&ehdr, sizeof(ehdr));
    if (file.gcount() != sizeof(ehdr)) return false;

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        std::cerr << "Invalid ELF header." << std::endl;
        return false;
    }

    uintptr_t minAddr = 0xFFFFFFFF;
    uintptr_t maxAddr = 0;

    std::vector<Elf32_Phdr> phdrs(ehdr.e_phnum);
    file.seekg(ehdr.e_phoff);
    file.read((char*)phdrs.data(), sizeof(Elf32_Phdr) * ehdr.e_phnum);

    for (const auto& phdr : phdrs) {
        if (phdr.p_type == PT_LOAD) {
            if (phdr.p_vaddr < minAddr) minAddr = phdr.p_vaddr;
            if (phdr.p_vaddr + phdr.p_memsz > maxAddr) maxAddr = phdr.p_vaddr + phdr.p_memsz;
        }
    }

    uintptr_t base, size;
    uintptr_t targetBase = minAddr;
    
    if (ehdr.e_type == 3) { // ET_DYN
         targetBase = manualBase;
    }

    if (!MemoryManager::ReserveAddressSpace(targetBase, maxAddr, base, size)) {
        std::cerr << "Failed to reserve memory: " << std::hex << targetBase << std::endl;
        return false;
    }
    
    loadedBase = base;
    loadedSize = size;

    log_info("ELF Type: %u (3=ET_DYN, 2=ET_EXEC), loadedBase=0x%p, loadedSize=0x%X", 
             ehdr.e_type, (void*)loadedBase, loadedSize);
    log_info("Loading ELF: %s", filePath.c_str());

    // Read section headers to map sections to segments
    std::vector<Elf32_Shdr> shdrs;
    std::vector<char> shstrtab;
    if (ehdr.e_shoff != 0 && ehdr.e_shentsize != 0 && ehdr.e_shnum > 0) {
        shdrs.resize(ehdr.e_shnum);
        file.clear();
        file.seekg(ehdr.e_shoff);
        file.read((char*)shdrs.data(), sizeof(Elf32_Shdr) * ehdr.e_shnum);
        
        // Read section header string table
        if (ehdr.e_shstrndx < ehdr.e_shnum) {
            Elf32_Shdr& shstr_hdr = shdrs[ehdr.e_shstrndx];
            shstrtab.resize(shstr_hdr.sh_size);
            file.clear();
            file.seekg(shstr_hdr.sh_offset);
            file.read(shstrtab.data(), shstr_hdr.sh_size);
        }
    }

    for (const auto& phdr : phdrs) {
        if (phdr.p_type == PT_LOAD) {
            log_info("PT_LOAD: VAddr: 0x%X, FileSz: 0x%X, MemSz: 0x%X, Offset: 0x%X", 
                     phdr.p_vaddr, phdr.p_filesz, phdr.p_memsz, phdr.p_offset);
            
            // Log which sections are in this segment
            for (size_t i = 0; i < shdrs.size(); ++i) {
                const auto& shdr = shdrs[i];
                if (shdr.sh_type != SHT_NULL && 
                    shdr.sh_offset >= phdr.p_offset && 
                    shdr.sh_offset < phdr.p_offset + phdr.p_filesz) {
                    const char* name = shstrtab.empty() ? "unknown" : (shstrtab.data() + shdr.sh_name);
                    log_info("  Section [%zu] %s is in this segment (offset=0x%X, size=%u)",
                             i, name, shdr.sh_offset, shdr.sh_size);
                    if (i == 25) {
                        log_info("    *** Section 25 (%s) WILL BE LOADED ***", name);
                        log_info("    Section 25 file range: 0x%X - 0x%X", 
                                 shdr.sh_offset, shdr.sh_offset + shdr.sh_size);
                        log_info("    Segment file range: 0x%X - 0x%X", 
                                 phdr.p_offset, phdr.p_offset + phdr.p_filesz);
                    }
                }
            }
            
            uintptr_t writeAddr = (ehdr.e_type == 3) ? (base + phdr.p_vaddr) : phdr.p_vaddr;
            MemoryManager::CommitSegment(writeAddr, phdr.p_memsz);
            void* dest = (void*)writeAddr;

            if (phdr.p_filesz > 0) {
                file.clear();
                file.seekg(phdr.p_offset);
                file.read((char*)dest, phdr.p_filesz);
                log_info("  Loaded 0x%X bytes from file offset 0x%X to 0x%p", 
                         phdr.p_filesz, phdr.p_offset, dest);
                
                // Check if section 25 is in this segment and dump its contents
                if (25 < shdrs.size()) {
                    const auto& sec25 = shdrs[25];
                    if (sec25.sh_offset >= phdr.p_offset && 
                        sec25.sh_offset < phdr.p_offset + phdr.p_filesz) {
                        uintptr_t sec25Addr = (uintptr_t)dest + (sec25.sh_offset - phdr.p_offset);
                        log_info("  Section 25 loaded at 0x%p, dumping first 64 bytes:", (void*)sec25Addr);
                        uint32_t* p = (uint32_t*)sec25Addr;
                        int nonZeroCount = 0;
                        for (int i = 0; i < 16; ++i) {
                            if (p[i] != 0) {
                                log_info("    [0x%X] 0x%08X", sec25.sh_addr + i*4, p[i]);
                                nonZeroCount++;
                            }
                        }
                        if (nonZeroCount == 0) {
                            log_error("  WARNING: Section 25 is ALL ZEROS after loading!");
                        }
                    }
                }
            }

            if (phdr.p_memsz > phdr.p_filesz) {
                memset((char*)dest + phdr.p_filesz, 0, phdr.p_memsz - phdr.p_filesz);
                log_info("  Zeroed 0x%X bytes (BSS) from 0x%p", 
                         phdr.p_memsz - phdr.p_filesz, (char*)dest + phdr.p_filesz);
            }
        }
        if (phdr.p_type == PT_DYNAMIC) {
            log_info("Found PT_DYNAMIC segment. FileSz: %u, Offset: %x", phdr.p_filesz, phdr.p_offset);
            size_t count = phdr.p_filesz / sizeof(Elf32_Dyn);
            log_info("Processing %u dynamic entries... (sizeof(Elf32_Dyn) = %zu)", count, sizeof(Elf32_Dyn));
            std::vector<Elf32_Dyn> dyns(count);

            file.clear(); // Reset EOF/Fail bits before seeking
            file.seekg(0, std::ios::end);
            size_t filesize = file.tellg();
            log_info("File Size: %zu", filesize);

            if (phdr.p_offset + phdr.p_filesz > filesize) {
                log_error("PT_DYNAMIC is past EOF! Offset: %x, Size: %u", phdr.p_offset, phdr.p_filesz);
            }

            file.clear(); // Reset again just in case
            file.seekg(phdr.p_offset);
            if (file.fail()) log_error("Seek failed to %x", phdr.p_offset);
            
            file.read((char*)dyns.data(), phdr.p_filesz);
            log_info("Read %ld bytes. Fail bit: %d", file.gcount(), file.fail());
            
            uintptr_t loadOffset = (ehdr.e_type == 3) ? base : 0;

            for (const auto& d : dyns) {
                log_info("Tag: 0x%X, Val: 0x%X", d.d_tag, d.d_un.d_val);
                if (d.d_tag == DT_SYMTAB) symTab = d.d_un.d_ptr + loadOffset;
                if (d.d_tag == DT_STRTAB) strTab = d.d_un.d_ptr + loadOffset;
                if (d.d_tag == DT_JMPREL) jmpRel = d.d_un.d_ptr + loadOffset;
                if (d.d_tag == DT_PLTRELSZ) pltRelSize = d.d_un.d_val;
                
                if (d.d_tag == DT_REL) rel = d.d_un.d_ptr + loadOffset;
                if (d.d_tag == DT_RELSZ) relSz = d.d_un.d_val;
                if (d.d_tag == DT_RELENT) relEnt = d.d_un.d_val;
                
                // Packed Relocations (DT_RELR)
                if (d.d_tag == DT_RELR) relr = d.d_un.d_ptr + loadOffset;
                if (d.d_tag == DT_RELRSZ) relrSz = d.d_un.d_val;
                if (d.d_tag == DT_RELRENT) relrEnt = d.d_un.d_val;

                // Hash Table
                if (d.d_tag == DT_HASH) {
                    hash = d.d_un.d_ptr + loadOffset;
                    log_info("Found DT_HASH for %s", filePath.c_str());
                }
                
                // GNU Hash (0x6ffffef5)
                if (d.d_tag == 0x6ffffef5) {
                    gnuHash = d.d_un.d_ptr + loadOffset;
                    log_info("Found DT_GNU_HASH for %s", filePath.c_str());
                }

                // Init/Fini
                if (d.d_tag == DT_INIT) initFunc = d.d_un.d_ptr + loadOffset;
                if (d.d_tag == DT_INIT_ARRAY) initArray = d.d_un.d_ptr + loadOffset;
                if (d.d_tag == DT_INIT_ARRAYSZ) initArraySz = d.d_un.d_val;
            }
            
             if (strTab != 0) {
                 for (const auto& d : dyns) {
                    if (d.d_tag == 1) { // DT_NEEDED
                        uintptr_t namePtr = strTab + d.d_un.d_val;
                        log_info("Parsing DT_NEEDED. StrTab: 0x%X, Val: 0x%X, Ptr: 0x%X", strTab, d.d_un.d_val, namePtr);
                        
                        const char* name = (const char*)namePtr;
                        // Determine max safe length or check if pointer is readable?
                        // For now just try to read
                        dependencies.push_back(name);
                         log_info("  Dependency: %s", name);
                    }
                 }
             }
        }
    }
    
    // Section headers already read above for PT_LOAD logging
    // Find SHT_DYNSYM to get symCount
    if (symCount == 0 && !shdrs.empty()) {
        for (const auto& shdr : shdrs) {
            if (shdr.sh_type == SHT_DYNSYM) {
                symCount = shdr.sh_size / sizeof(Elf32_Sym);
                log_info("Found SHT_DYNSYM. Count: %u", symCount);
                break;
            }
        }
    }
    
    // Log section 25 details if it exists
    if (25 < shdrs.size()) {
        const char* name = shstrtab.empty() ? "unknown" : (shstrtab.data() + shdrs[25].sh_name);
        log_info("Section 25: %s, type=%u, flags=0x%X, addr=0x%X, offset=0x%X, size=%u",
                 name, shdrs[25].sh_type, shdrs[25].sh_flags, shdrs[25].sh_addr, 
                 shdrs[25].sh_offset, shdrs[25].sh_size);
    }
    
    // Fallback: If SHT_DYNSYM was not found (stripped) but DT_HASH is present
    if (symCount == 0 && hash != 0) {
        uint32_t* hashData = (uint32_t*)hash;
        symCount = hashData[1]; // nchain
        log_info("Derived symCount %u from DT_HASH nchain", symCount);
    }
    
    // Fallback: If symCount is still 0 and DT_GNU_HASH is present
    if (symCount == 0 && gnuHash != 0) {
        log_info("Attempting to derive symCount from GNU Hash. Base: 0x%X, gnuHash: 0x%X", loadedBase, gnuHash);
        
        uint32_t* gnu = (uint32_t*)gnuHash;
        uint32_t nbuckets = gnu[0];
        uint32_t symndx = gnu[1];
        uint32_t maskwords = gnu[2];
        // shift2 = gnu[3];
        
        log_info("GNU Hash: nbuckets=%u, symndx=%u, maskwords=%u", nbuckets, symndx, maskwords);

        uint32_t* bloom = gnu + 4;
        uint32_t* buckets = bloom + maskwords;
        uint32_t* chains = buckets + nbuckets;
        
        uint32_t max_sym = 0;
        for (uint32_t i = 0; i < nbuckets; ++i) {
             if (buckets[i] > max_sym) {
                 max_sym = buckets[i];
             }
        }
        
        log_info("GNU Hash: max_sym from buckets = %u", max_sym);
        
        if (max_sym >= symndx) {
            // max_sym is the start of the chain for the last bucket.
            // Walk the chain until LSB is 1.
            uint32_t* chain_ptr = chains + (max_sym - symndx);
            while (true) {
                uint32_t hashVal = *chain_ptr;
                max_sym++;
                if (hashVal & 1) break;
                chain_ptr++;
            }
            symCount = max_sym;
            log_info("Derived symCount %u from DT_GNU_HASH", symCount);
        } else {
             // If buckets are empty, maybe symndx is the count? 
             // Or there are only local symbols?
             // Usually symndx is the start of hashed symbols.
             if (symndx > 0) {
                 symCount = symndx;
                 log_info("Derived symCount %u (only local/unhashed symbols?) from symndx", symCount);
             }
        }
    }

    if (ehdr.e_type == 3) {
        if (!Relocate(loadedBase)) {
            std::cerr << "Relocation failed!" << std::endl;
            return false;
        }
    }

    // Log some symbol table info for debugging
    if (symTab != 0 && symCount > 0) {
        log_info("Symbol table dump (first 10 symbols):");
        for (uint32_t i = 0; i < symCount && i < 10; ++i) {
            Elf32_Sym* sym = (Elf32_Sym*)(symTab + i * sizeof(Elf32_Sym));
            const char* name = (const char*)(strTab + sym->st_name);
            if (name && (strncmp(name, "_ZTV", 4) == 0 || strncmp(name, "_ZTT", 4) == 0)) {
                log_info("  [%u] %s: shndx=%u, value=0x%X, size=%u", 
                         i, name, sym->st_shndx, sym->st_value, sym->st_size);
            }
        }
    }

    return true;
}

bool ElfLoader::Relocate(uintptr_t baseAddr) {
    if (rel == 0 || relSz == 0) return true;

    size_t count = relSz / (relEnt ? relEnt : sizeof(Elf32_Rel));

    log_info("ElfLoader::Relocate: Processing %zu relocations at rel=0x%p, baseAddr=0x%p", count, (void*)rel, (void*)baseAddr);

    for (size_t i = 0; i < count; ++i) {
        Elf32_Rel* r = (Elf32_Rel*)(rel + i * sizeof(Elf32_Rel));
        int type = ELF32_R_TYPE(r->r_info);
        uint32_t symIdx = ELF32_R_SYM(r->r_info);

        if (type == R_386_RELATIVE) {
            uintptr_t* target = (uintptr_t*)(baseAddr + r->r_offset);
            uintptr_t addend = *target;
            
            // Log if this might be a vtable (addend looks like an offset)
            if (addend > 0x1000 && addend < 0x1000000) {
                log_info("  R_386_RELATIVE[%zu]: offset=0x%X, addend=0x%X, target=0x%p -> 0x%p",
                         i, r->r_offset, addend, target, (void*)(baseAddr + addend));
            }

            DWORD oldProtect;
            if (VirtualProtect((LPVOID)target, 4, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                *target = baseAddr + addend;
                VirtualProtect((LPVOID)target, 4, oldProtect, &oldProtect);
            } else {
                 return false;
            }
        }
    }

    // Pass 2: Process R_386_IRELATIVE (IFUNC)
    // These must be processed AFTER R_386_RELATIVE because the resolver code itself might
    // have been relocated (e.g. GOT usage).
    for (size_t i = 0; i < count; ++i) {
        Elf32_Rel* r = (Elf32_Rel*)(rel + i * sizeof(Elf32_Rel));
        int type = ELF32_R_TYPE(r->r_info);

        if (type == R_386_IRELATIVE) {
            uintptr_t* target = (uintptr_t*)(baseAddr + r->r_offset);
            
            // The value at *target is the address of the resolver function (plus base addr)
            // Actually, for IRELATIVE, the addend is stored at the location.
            // Symbol value is ignored.
            // Resolver Address = Base + *target
            
            uintptr_t resolverOffset = *target;
            uintptr_t resolverAddr = baseAddr + resolverOffset;
            
            log_info("Processing R_386_IRELATIVE at offset %x. Resolver: %p (Offset: %x)", r->r_offset, (void*)resolverAddr, resolverOffset);

            // Call the resolver
            typedef uintptr_t (*ResolverFunc)();
            ResolverFunc resolver = (ResolverFunc)resolverAddr;
            uintptr_t realFunc = resolver();
            
            log_info("  -> Resolved to: %p", (void*)realFunc);

            // Write back the real function address to the GOT entry
            DWORD oldProtect;
            if (VirtualProtect((LPVOID)target, 4, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                *target = realFunc;
                VirtualProtect((LPVOID)target, 4, oldProtect, &oldProtect);
            } else {
                 log_error("VirtualProtect failed for IRELATIVE target %p", target);
                 return false;
            }
        }
    }
    
    // Process DT_RELR (Packed Relocations)
    if (relr != 0 && relrSz != 0) {
        log_info("Processing DT_RELR (Packed Relocations). Size: %u", relrSz);
        uintptr_t* entries = (uintptr_t*)relr;
        size_t count = relrSz / sizeof(uintptr_t);
        uintptr_t base = 0;
        
        for (size_t i = 0; i < count; ++i) {
            uintptr_t entry = entries[i];
            
            if ((entry & 1) == 0) {
                // Even entry: Address (Base pointer)
                base = entry + baseAddr;
                // Relocate the address itself (it points to a pointer)
                *(uintptr_t*)base += baseAddr;
                base += sizeof(uintptr_t);
            } else {
                // Odd entry: Bitmap
                // Each bitmap covers exactly (BITS_PER_WORD - 1) words.
                // On 32-bit: 31 words per bitmap entry.
                // We MUST advance base by all 31 words regardless of bit values,
                // otherwise subsequent entries will be misaligned.
                constexpr int BitsPerBitmap = sizeof(uintptr_t) * 8 - 1; // 31 on 32-bit
                for (int bit = 0; bit < BitsPerBitmap; ++bit) {
                    if ((entry >> (bit + 1)) & 1) {
                        *(uintptr_t*)base += baseAddr;
                    }
                    base += sizeof(uintptr_t);
                }
            }
        }
    }

    return true; 
}

uintptr_t ElfLoader::GetSymbolAddress(const char* name) const {
    // log_trace("ElfLoader(%s): Searching for '%s'", filePath.c_str(), name);
    if (symTab == 0 || strTab == 0) return 0;

    // For ET_DYN, st_value is relative to load base — add loadedBase.
    // For ET_EXEC, st_value is already absolute — add nothing.
    uintptr_t symbolBias = (ehdr.e_type == 3) ? loadedBase : 0;
    
    // Check if this is a vtable symbol for logging
    bool isVtable = (strncmp(name, "_ZTV", 4) == 0 || strncmp(name, "_ZTT", 4) == 0);

    // Strategy 1: DT_GNU_HASH (Preferred if present)
    if (gnuHash != 0) {
        const uint32_t* buckets;
        const uint32_t* chains;
        const uint32_t* bloom;
        uint32_t nbuckets, symndx, maskwords, shift2;

        uint32_t* gnu = (uint32_t*)gnuHash;
        nbuckets = gnu[0];
        symndx = gnu[1];
        maskwords = gnu[2];
        shift2 = gnu[3];

        bloom = gnu + 4;
        buckets = bloom + maskwords;
        chains = buckets + nbuckets;

        if (maskwords == 0 || nbuckets == 0) return 0; // Safety Check

        uint32_t h = dl_new_hash(name);

        // Bloom Filter Check
        uint32_t bloom_word = bloom[(h / 32) % maskwords];
        uint32_t mask = (1 << (h % 32)) | (1 << ((h >> shift2) % 32));
        if ((bloom_word & mask) != mask) {
            // Definitely not in this library
            if (isVtable) log_debug("  GNU Hash bloom filter rejected '%s'", name);
            return 0;
        }

        uint32_t bucket = buckets[h % nbuckets];
        if (bucket == 0) return 0;

        const uint32_t* chain = chains + (bucket - symndx);
        const Elf32_Sym* sym = (Elf32_Sym*)(symTab + bucket * sizeof(Elf32_Sym));

        // Loop through the chain
        do {
            uint32_t hash = *chain;
            if ((hash & ~1) == (h & ~1)) {
                // Hash matches, check name
                const char* symName = (const char*)(strTab + sym->st_name);

                size_t len = strlen(name);
                if (strncmp(name, symName, len) == 0) {
                    if (symName[len] == '\0' || symName[len] == '@') {
                        if (isVtable) {
                            log_info("  [GETSYM] Found '%s' in GNU hash: shndx=%u, type=%u, bind=%u, value=0x%X",
                                     name, sym->st_shndx, ELF32_ST_TYPE(sym->st_info), 
                                     ELF32_ST_BIND(sym->st_info), sym->st_value);
                        }
                        if (sym->st_shndx != 0 && ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
                             uintptr_t value = symbolBias + sym->st_value;
                             if (ELF32_ST_TYPE(sym->st_info) == STT_GNU_IFUNC) {
                                 typedef uintptr_t (*ResolverFunc)();
                                 value = ((ResolverFunc)value)();
                             }
                             return value;
                        }
                        else if (isVtable)
                        {
                            log_info("  [GETSYM] Rejected: shndx=%u, bind=%u", sym->st_shndx, ELF32_ST_BIND(sym->st_info));
                        }
                    }
                }
            }
             // Check LSB (1 means end of chain)
            if (hash & 1) break;

            ++sym;
            ++chain;
        } while (true);
    }
    
    // Strategy 2: DT_HASH (if available)
    if (hash != 0) {
        uint32_t* hashData = (uint32_t*)hash;
        uint32_t nbucket = hashData[0];
        uint32_t nchain = hashData[1];
        uint32_t* bucket = &hashData[2];
        uint32_t* chain = &hashData[2 + nbucket];
    
        uint32_t hashVal = elf_hash((const unsigned char*)name);
        uint32_t index = bucket[hashVal % nbucket];
    
        while (index != 0 && index < nchain) {
            Elf32_Sym* sym = (Elf32_Sym*)(symTab + index * sizeof(Elf32_Sym));
            const char* symName = (const char*)(strTab + sym->st_name);
    
            // Check for versioned symbols
            size_t len = strlen(name);
            if (strncmp(name, symName, len) == 0) {
                if (symName[len] == '\0' || symName[len] == '@') {
                    if (sym->st_shndx != 0 && ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
                         return symbolBias + sym->st_value; 
                    }
                }
            }
            index = chain[index];
        }
    }
    
    // Strategy 3: Linear Scan (Fallback if DT_HASH missing or lookups fail)
    // This is O(N) but reliable if we know symCount.
    if (symCount > 0) {
        // log_warn("Using Linear Scan for %s in %s (Hash failed or missing)", name, filePath.c_str());
        for (uint32_t i = 0; i < symCount; ++i) {
             Elf32_Sym* sym = (Elf32_Sym*)(symTab + i * sizeof(Elf32_Sym));
             const char* symName = (const char*)(strTab + sym->st_name);

             // Check for versioned symbols
             size_t len = strlen(name);
             if (strncmp(name, symName, len) == 0) {
                if (symName[len] == '\0' || symName[len] == '@') {
                    if (isVtable) {
                        log_info("  [GETSYM] Linear scan found '%s': shndx=%u, type=%u, bind=%u, value=0x%X, size=%u",
                                 name, sym->st_shndx, ELF32_ST_TYPE(sym->st_info),
                                 ELF32_ST_BIND(sym->st_info), sym->st_value, sym->st_size);
                    }
                    if (sym->st_shndx != 0 && ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
                         return symbolBias + sym->st_value;
                    }
                    else if (isVtable)
                    {
                        log_info("  [GETSYM] Linear scan rejected: shndx=%u, bind=%u", 
                                 sym->st_shndx, ELF32_ST_BIND(sym->st_info));
                    }
                }
             }
        }
    }

    if (isVtable) {
        log_debug("  [GETSYM] '%s' NOT found in %s (symCount=%u, gnuHash=%p, hash=%p)", 
                  name, filePath.c_str(), symCount, (void*)gnuHash, (void*)hash);
    }
    return 0;
}

// Helper: Check if an address is within a loaded range and has execute permission.
static bool IsExecutableAddress(uintptr_t addr, uintptr_t base, uintptr_t size)
{
    if (addr < base || addr >= (base + size))
        return false;

    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)))
        return false;

    const DWORD execFlags = PAGE_EXECUTE | PAGE_EXECUTE_READ
                          | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & execFlags) != 0;
}

// Helper: Call a function pointer with exception handling, logging the label.

// Thread-local context for VEH crash reporting
static thread_local const char *g_currentInitLabel = nullptr;
static thread_local uintptr_t   g_currentInitFunc  = 0;

static LONG WINAPI InitVectoredHandler(EXCEPTION_POINTERS *ep)
{
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    void *addr = ep->ExceptionRecord->ExceptionAddress;

    // Only handle access violations and similar fatal exceptions
    if (code == EXCEPTION_ACCESS_VIOLATION ||
        code == EXCEPTION_STACK_OVERFLOW ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_PRIV_INSTRUCTION)
    {
        log_fatal("=== FATAL EXCEPTION IN %s ===", g_currentInitLabel ? g_currentInitLabel : "unknown");
        log_fatal("  Exception Code: 0x%08X at address %p", code, addr);
        log_fatal("  Init function was at: %p", (void *)g_currentInitFunc);

        if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
        {
            ULONG_PTR readWrite = ep->ExceptionRecord->ExceptionInformation[0];
            ULONG_PTR targetAddr = ep->ExceptionRecord->ExceptionInformation[1];
            log_fatal("  Access Violation: %s address 0x%08X",
                      readWrite == 0 ? "reading" : (readWrite == 1 ? "writing" : "executing"),
                      (unsigned int)targetAddr);
        }

        log_fatal("=== Check symbol resolution log above for stubbed symbols ===");

        // Show message box so user sees it
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "FATAL: Exception 0x%08X in %s\n"
                 "Address: %p\n"
                 "Init function: %p\n\n"
                 "Check log for stubbed symbols.",
                 (unsigned int)code,
                 g_currentInitLabel ? g_currentInitLabel : "unknown",
                 addr, (void *)g_currentInitFunc);
        MessageBoxA(NULL, msg, "ELF Init Crash", MB_OK | MB_ICONERROR);

        TerminateProcess(GetCurrentProcess(), 1);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

static void CallInitSafe(uintptr_t func, const char *label)
{
    typedef void (*InitFuncPtr)();

    g_currentInitLabel = label;
    g_currentInitFunc  = func;

    // Install VEH (first handler) for crash catching on MinGW
    PVOID veh = AddVectoredExceptionHandler(1, InitVectoredHandler);

#ifdef _MSC_VER
    __try {
        ((InitFuncPtr)func)();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_error("  SEH Exception 0x%08X in %s at %p", GetExceptionCode(), label, (void *)func);
    }
#else
    ((InitFuncPtr)func)();
#endif

    if (veh)
        RemoveVectoredExceptionHandler(veh);

    g_currentInitLabel = nullptr;
    g_currentInitFunc  = 0;
}

void ElfLoader::RunInit()
{
    log_info("RunInit(%s): Starting initialization...", filePath.c_str());

    // 1. DT_INIT
    if (initFunc != 0)
    {
        if (IsExecutableAddress(initFunc, loadedBase, loadedSize))
        {
            log_info("  Calling DT_INIT at %p for %s", (void *)initFunc, filePath.c_str());
            CallInitSafe(initFunc, "DT_INIT");
            log_info("  DT_INIT completed for %s", filePath.c_str());
        }
        else
        {
            log_warn("  DT_INIT at %p is not executable (base: %p, size: 0x%X)",
                     (void *)initFunc, (void *)loadedBase, loadedSize);
        }
    }

    // 2. DT_INIT_ARRAY
    if (initArray != 0 && initArraySz > 0)
    {
        uint32_t count = initArraySz / 4; // 32-bit pointers

        if (initArray < loadedBase || initArray >= (loadedBase + loadedSize))
        {
            log_warn("  INIT_ARRAY at %p is outside loaded range", (void *)initArray);
        }
        else
        {
            uintptr_t *funcs = (uintptr_t *)initArray;
            log_info("  Processing INIT_ARRAY (%u entries) for %s", count, filePath.c_str());

            for (uint32_t i = 0; i < count; ++i)
            {
                if (funcs[i] == 0 || funcs[i] == (uintptr_t)-1)
                    continue;

                if (!IsExecutableAddress(funcs[i], loadedBase, loadedSize))
                {
                    log_warn("  INIT_ARRAY[%d] at %p is not executable", i, (void *)funcs[i]);
                    continue;
                }

                char label[32];
                snprintf(label, sizeof(label), "INIT_ARRAY[%d]", i);
                log_info("  Calling %s at %p", label, (void *)funcs[i]);
                CallInitSafe(funcs[i], label);
                log_info("  %s completed", label);
            }
        }
    }

    log_info("RunInit(%s): Initialization complete.", filePath.c_str());
}

void ElfLoader::ResolveSymbols() {
    uintptr_t relocationBase = (ehdr.e_type == 3) ? loadedBase : 0;
    SymbolResolver::ResolveAll(jmpRel, rel, symTab, strTab, pltRelSize, relSz, relEnt, relocationBase);
}

void ElfLoader::DebugPrintSymbols() const {
    // If gnuHash is present but symCount is 0, we can try to estimate symCount 
    // or just say "GNU Hash present but Linear Scan unavailable".
    
    if (gnuHash != 0 && symCount == 0) {
        log_warn("DT_GNU_HASH present but symCount is 0 (SHT_DYNSYM stripped). Cannot list symbols linearly.");
    }

    log_info("--- Symbols in %s (Count: %u) ---", filePath.c_str(), symCount);
    if (symCount == 0) return;
    
    if (symTab == 0 || strTab == 0) {
        log_warn("SymTab or StrTab is missing, cannot dump!");
        return;
    }

    for (uint32_t i = 0; i < symCount; ++i) {
        Elf32_Sym* sym = (Elf32_Sym*)(symTab + i * sizeof(Elf32_Sym));
        const char* name = (const char*)(strTab + sym->st_name);
        // Clean output: only global/weak, and valid names
        if (name && *name && ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
             log_info("  [%u] %s @ 0x%X (Shndx: %u)", i, name, sym->st_value, sym->st_shndx);
        }
    }
    log_info("-----------------------------------");
}
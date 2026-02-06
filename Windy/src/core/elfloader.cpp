#include "elfloader.h"
#include "memorymanager.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>
#include "log.h"

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

    for (const auto& phdr : phdrs) {
        if (phdr.p_type == PT_LOAD) {
            log_info("PT_LOAD: VAddr: 0x%X, FileSz: 0x%X, MemSz: 0x%X", phdr.p_vaddr, phdr.p_filesz, phdr.p_memsz);
            uintptr_t writeAddr = (ehdr.e_type == 3) ? (base + phdr.p_vaddr) : phdr.p_vaddr;
            MemoryManager::CommitSegment(writeAddr, phdr.p_memsz);
            void* dest = (void*)writeAddr;

            if (phdr.p_filesz > 0) {
                file.clear();
                file.seekg(phdr.p_offset);
                file.read((char*)dest, phdr.p_filesz);
            }

            if (phdr.p_memsz > phdr.p_filesz) {
                memset((char*)dest + phdr.p_filesz, 0, phdr.p_memsz - phdr.p_filesz);
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
    
    // Read Section Headers to find SHT_DYNSYM (Symbol Table)
    if (ehdr.e_shoff != 0 && ehdr.e_shentsize != 0) {
        log_info("Reading Section Headers. Offset: %u, Num: %u", ehdr.e_shoff, ehdr.e_shnum);
        
        file.clear(); // Reset before seek
        file.seekg(ehdr.e_shoff);
        if (file.fail()) {
             log_error("Seek failed to Section Headers at %u", ehdr.e_shoff);
        } else {
            std::vector<Elf32_Shdr> shdrs(ehdr.e_shnum);
            file.read((char*)shdrs.data(), sizeof(Elf32_Shdr) * ehdr.e_shnum);
            
            for (const auto& shdr : shdrs) {
                if (shdr.sh_type == SHT_DYNSYM) {
                    symCount = shdr.sh_size / sizeof(Elf32_Sym); // or sh_entsize
                    log_info("Found SHT_DYNSYM. Count: %u", symCount);
                    break;
                }
            }
        }
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
    
    return true;
}

bool ElfLoader::Relocate(uintptr_t baseAddr) {
    if (rel == 0 || relSz == 0) return true;

    size_t count = relSz / (relEnt ? relEnt : sizeof(Elf32_Rel));
    
    for (size_t i = 0; i < count; ++i) {
        Elf32_Rel* r = (Elf32_Rel*)(rel + i * sizeof(Elf32_Rel));
        int type = ELF32_R_TYPE(r->r_info);

        if (type == R_386_RELATIVE) {
            uintptr_t* target = (uintptr_t*)(baseAddr + r->r_offset);
            
            DWORD oldProtect;
            if (VirtualProtect((LPVOID)target, 4, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                *target += baseAddr;
                VirtualProtect((LPVOID)target, 4, oldProtect, &oldProtect);
            } else {
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
                // Each bit applies to next words
                uintptr_t bitmap = entry >> 1;
                while (bitmap != 0) {
                    if (bitmap & 1) {
                        *(uintptr_t*)base += baseAddr;
                    }
                    bitmap >>= 1;
                    base += sizeof(uintptr_t);
                }
                // Skip padding words if using 64-bit bitmap on 32-bit? 
                // Wait, on 32-bit, uintptr_t is 32-bit. Bitmap covers 31 words.
                // Loop correctly advances base.
            }
        }
    }

    return true; 
}

uintptr_t ElfLoader::GetSymbolAddress(const char* name) const {
    if (symTab == 0 || strTab == 0) return 0;
    
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
                         if (sym->st_shndx != 0 && ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
                              return loadedBase + sym->st_value; 
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
                         return loadedBase + sym->st_value; 
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
                    if (sym->st_shndx != 0 && ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
                         return loadedBase + sym->st_value; 
                    }
                }
             }
        }
    }
    
    return 0; 
}

void ElfLoader::RunInit() {
    // 1. DT_INIT
    if (initFunc != 0) {
        // cast to function pointer and call
        typedef void (*InitFunc)();
        InitFunc func = (InitFunc)initFunc;
        // log_info("Calling DT_INIT at %p", func);
        func();
    }

    // 2. DT_INIT_ARRAY
    if (initArray != 0 && initArraySz > 0) {
        uint32_t count = initArraySz / 4; // Assuming 32-bit pointers
        uintptr_t* funcs = (uintptr_t*)initArray;
        for (uint32_t i = 0; i < count; ++i) {
            if (funcs[i] != 0 && funcs[i] != (uintptr_t)-1) {
                 typedef void (*InitFunc)();
                 // Values in INIT_ARRAY for ET_DYN are RELATIVE OFFSET? Or relocated absolute?
                 // After Relocate(), they should be ABSOLUTE.
                 InitFunc func = (InitFunc)funcs[i];
                 // log_info("Calling INIT_ARRAY[%d] at %p", i, func);
                 func();
            }
        }
    }
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
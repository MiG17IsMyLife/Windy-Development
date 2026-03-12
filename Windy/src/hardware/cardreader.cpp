#define _CRT_SECURE_NO_WARNINGS

#include "cardreader.h"
#include "../core/config.h"
#include "../core/common.h"
#include "../core/log.h"
#include "../core/patches.h"
#include "../minhook/include/MinHook.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <io.h>

// ============================================================
// Global state
// ============================================================
bool g_triggerInsertKey = false;

static int  s_idCardSize = 0;
static bool s_idTwoDigitsCardCount = false;

// ============================================================
// FindStaticFnAddr - search ELF .symtab for mangled C++ names
// ============================================================
void* FindStaticFnAddr(const char* functionName)
{
    const char* elfPath = ConfigManager::Instance().GetElfPath();
    if (!elfPath || elfPath[0] == '\0')
    {
        log_error("FindStaticFnAddr: No ELF path set");
        return nullptr;
    }

    FILE* f = fopen(elfPath, "rb");
    if (!f)
    {
        log_error("FindStaticFnAddr: Cannot open ELF file: %s", elfPath);
        return nullptr;
    }

    // Read ELF header
    Elf32_Ehdr ehdr;
    fread(&ehdr, sizeof(ehdr), 1, f);

    // Read section headers
    size_t shdrSize = ehdr.e_shnum * sizeof(Elf32_Shdr);
    Elf32_Shdr* shdrs = (Elf32_Shdr*)malloc(shdrSize);
    fseek(f, ehdr.e_shoff, SEEK_SET);
    fread(shdrs, sizeof(Elf32_Shdr), ehdr.e_shnum, f);

    // Read section header string table
    Elf32_Shdr* shstrHdr = &shdrs[ehdr.e_shstrndx];
    char* shstrtab = (char*)malloc(shstrHdr->sh_size);
    fseek(f, shstrHdr->sh_offset, SEEK_SET);
    fread(shstrtab, 1, shstrHdr->sh_size, f);

    // Find .symtab and .strtab
    Elf32_Shdr* symtabHdr = nullptr;
    Elf32_Shdr* strtabHdr = nullptr;

    for (int i = 0; i < ehdr.e_shnum; i++)
    {
        const char* secName = shstrtab + shdrs[i].sh_name;
        if (strcmp(secName, ".symtab") == 0)
            symtabHdr = &shdrs[i];
        else if (strcmp(secName, ".strtab") == 0)
            strtabHdr = &shdrs[i];
    }

    void* result = nullptr;

    if (symtabHdr && strtabHdr)
    {
        // Read symbol table
        Elf32_Sym* symtab = (Elf32_Sym*)malloc(symtabHdr->sh_size);
        fseek(f, symtabHdr->sh_offset, SEEK_SET);
        fread(symtab, 1, symtabHdr->sh_size, f);

        // Read string table
        char* strtab = (char*)malloc(strtabHdr->sh_size);
        fseek(f, strtabHdr->sh_offset, SEEK_SET);
        fread(strtab, 1, strtabHdr->sh_size, f);

        int numSyms = symtabHdr->sh_size / sizeof(Elf32_Sym);
        for (int i = 0; i < numSyms; i++)
        {
            const char* symName = strtab + symtab[i].st_name;
            if (strcmp(symName, functionName) == 0 && symtab[i].st_value != 0)
            {
                result = (void*)(uintptr_t)symtab[i].st_value;
                log_info("FindStaticFnAddr: Found '%s' at 0x%08X", functionName, symtab[i].st_value);
                break;
            }
        }

        free(strtab);
        free(symtab);
    }
    else
    {
        log_warn("FindStaticFnAddr: .symtab or .strtab not found in ELF (stripped binary?)");
    }

    free(shstrtab);
    free(shdrs);
    fclose(f);

    if (!result)
        log_warn("FindStaticFnAddr: Symbol '%s' not found", functionName);

    return result;
}

// ============================================================
// checkTrgOn callback - replaces _sInterface::checkTrgOn
// ============================================================
static bool checkTrgOn(int param1, long param2)
{
    if (param2 == 0x1000 && g_triggerInsertKey)
    {
        if (IdCardFileExists("", s_idCardSize, s_idTwoDigitsCardCount))
            return true;
    }
    return false;
}

// ============================================================
// IdCardFileExists
// ============================================================
bool IdCardFileExists(const char* folderPath, long expectedSize, bool twoDigits)
{
    char fileName[64];
    char fullPath[1024];
    const char* fmt;
    int maxCards = twoDigits ? 99 : 999;
    const char* cardFmt = twoDigits ? "InidCard%02d.crd" : "InidCrd%03d.crd";

    for (int i = 0; i <= maxCards; i++)
    {
        sprintf(fileName, cardFmt, i);

        if (folderPath && strlen(folderPath) > 0)
        {
            char lastChar = folderPath[strlen(folderPath) - 1];
            if (lastChar == '/' || lastChar == '\\')
                sprintf(fullPath, "%s%s", folderPath, fileName);
            else
                sprintf(fullPath, "%s\\%s", folderPath, fileName);
        }
        else
        {
            strcpy(fullPath, fileName);
        }

        struct _stat fileStat;
        if (_stat(fullPath, &fileStat) == 0)
        {
            if (fileStat.st_size == expectedSize)
            {
                log_info("IdCardFileExists: Found card file '%s' (size=%ld)", fullPath, (long)fileStat.st_size);
                return true;
            }
        }
    }
    return false;
}

// ============================================================
// IdPatchEject
// ============================================================
void IdPatchEject()
{
    GameID gid = getConfig()->gameId;

    switch (gid)
    {
    case INITIALD_5_JAP_REVA:
        Patches::patchMemoryFromString(0x083d92bd, "84"); // tickInsertWait
        Patches::patchMemoryFromString(0x083d92f1, "85");
        break;
    case INITIALD_5_JAP_REVC:
        Patches::patchMemoryFromString(0x083d96fd, "84");
        Patches::patchMemoryFromString(0x083d9731, "85");
        break;
    case INITIALD_5_JAP_REVF:
        Patches::patchMemoryFromString(0x083ec6b9, "84");
        Patches::patchMemoryFromString(0x083ec6eb, "85");
        break;
    case INITIALD_5_EXP:
        Patches::patchMemoryFromString(0x083e1eb9, "84");
        Patches::patchMemoryFromString(0x083e1eeb, "85");
        break;
    case INITIALD_5_EXP_20:
        Patches::patchMemoryFromString(0x083ebd1b, "84");
        Patches::patchMemoryFromString(0x083ebd51, "85");
        break;
    case INITIALD_5_EXP_20A:
        Patches::patchMemoryFromString(0x083ec07b, "84");
        Patches::patchMemoryFromString(0x083ec0b1, "85");
        break;
    default:
        break;
    }
}

// idWriteFileHeader - stored function pointer from game
static void (*s_idWriteFileHeader)(void*) = nullptr;

static void idWriteFileHeaderTram(void* param1)
{
    IdPatchEject();
    if (s_idWriteFileHeader)
        s_idWriteFileHeader(param1);
}

// ============================================================
// ApplyCardReaderPatches
// ============================================================

void ApplyCardReaderPatches()
{
    GameID    gid = getConfig()->gameId;
    GameGroup grp = getConfig()->gameGroup;

    // Only ID4/ID5 games support card reader
    if (grp != GROUP_ID4_JAP && grp != GROUP_ID4_EXP && grp != GROUP_ID5)
    {
        log_info("CardReader: Game does not use ID card reader");
        return;
    }

    log_info("CardReader: Applying card reader patches for game %d", (int)gid);

    switch (gid)
    {
    case INITIALD_5_JAP_REVA:
    {
        Patches::patchMemoryFromString(0x0828471a, "889b9b"); // RealCard --> FileCard
        Patches::patchMemoryFromString(0x081ce9a3, "eb");     // Fix crash in findCardFiles

        void* emulationAddr = FindStaticFnAddr("_ZN11cFileCardIF9emulationEv");
        if (emulationAddr)
            Patches::patchMemoryFromString((uintptr_t)emulationAddr + 0x21, "74");

        void* checkTrgOnAddr = FindStaticFnAddr("_ZN11_sInterface10checkTrgOnENS_11PORT_NUMBEREm");
        if (checkTrgOnAddr)
            MH_CreateHook(checkTrgOnAddr, (void*)checkTrgOn, nullptr);

        s_idCardSize = 1354;
        s_idTwoDigitsCardCount = false;
        log_info("CardReader: ID5 Rev A patches applied (cardSize=%d)", s_idCardSize);
        break;
    }
    case INITIALD_5_JAP_REVC:
    {
        Patches::patchMemoryFromString(0x08284dea, "889b9b");
        Patches::patchMemoryFromString(0x081cea73, "eb");

        void* emulationAddr = FindStaticFnAddr("_ZN11cFileCardIF9emulationEv");
        if (emulationAddr)
            Patches::patchMemoryFromString((uintptr_t)emulationAddr + 0x21, "74");

        void* checkTrgOnAddr = FindStaticFnAddr("_ZN11_sInterface10checkTrgOnENS_11PORT_NUMBEREm");
        if (checkTrgOnAddr)
            MH_CreateHook(checkTrgOnAddr, (void*)checkTrgOn, nullptr);

        s_idCardSize = 1354;
        s_idTwoDigitsCardCount = false;
        log_info("CardReader: ID5 Rev C patches applied");
        break;
    }
    case INITIALD_5_JAP_REVF:
    {
        Patches::patchMemoryFromString(0x08295e0a, "889b9b");
        Patches::patchMemoryFromString(0x081cf613, "eb");

        void* emulationAddr = FindStaticFnAddr("_ZN11cFileCardIF9emulationEv");
        if (emulationAddr)
            Patches::patchMemoryFromString((uintptr_t)emulationAddr + 0x21, "74");

        void* checkTrgOnAddr = FindStaticFnAddr("_ZN11_sInterface10checkTrgOnENS_11PORT_NUMBEREm");
        if (checkTrgOnAddr)
            MH_CreateHook(checkTrgOnAddr, (void*)checkTrgOn, nullptr);

        s_idCardSize = 1354;
        s_idTwoDigitsCardCount = false;
        log_info("CardReader: ID5 Rev F patches applied");
        break;
    }
    default:
        log_warn("CardReader: No card reader patches for game ID %d", (int)gid);
        break;
    }
}

#include "core/ElfLoader.h"
#include "core/SymbolResolver.h"
#include "core/LinuxStack.h"
#include "core/log.h"
#include <iostream>
#include <windows.h>
#include <objbase.h>

static const size_t STACK_SIZE = 1024 * 1024;
static const char* DEFAULT_ARGV0 = "id5.elf";

int main(int argc, char* argv[]) {

    // Initialize logging system first
    logInit();

    log_info("==============================================");
    log_info("   Windy - Lindbergh Loader for Windows");
    log_info("==============================================");

    // COM initialization
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        log_warn("CoInitializeEx failed: 0x%08X", hr);
    }
    else {
        log_debug("COM initialized successfully");
    }

    if (argc < 2) {
        log_error("No ELF file path specified.");
        log_info("Usage: Windy.exe <path_to_elf>");
        system("pause");
        return 1;
    }

    log_info("Loading ELF: %s", argv[1]);

    ElfLoader loader(argv[1]);
    if (!loader.LoadToMemory()) {
        log_fatal("Failed to load ELF file into memory.");
        system("pause");
        return 1;
    }

    log_info("ELF loaded successfully, resolving symbols...");

    // Perform dynamic linking (GOT patching)
    SymbolResolver::ResolveAll(
        loader.GetJmpRel(),
        loader.GetSymTab(),
        loader.GetStrTab(),
        loader.GetPltRelSize()
    );

    log_info("Symbol resolution complete.");

    // Prepare Linux-style stack
    uint32_t finalEsp = LinuxStack::Setup(STACK_SIZE, DEFAULT_ARGV0);
    uint32_t entry = loader.GetHeader().e_entry;

    log_info("Jumping to ELF entry point: 0x%08X", entry);
    log_info("==============================================");

    // Switch stack and jump to ELF entry
    __asm {
        mov esp, finalEsp
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        jmp entry
    }

    return 0;
}
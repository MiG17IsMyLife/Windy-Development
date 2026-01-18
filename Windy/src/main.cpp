#include "core/ElfLoader.h"
#include "core/SymbolResolver.h"
#include "core/LinuxStack.h"
#include <iostream>
#include <windows.h>
#include <objbase.h>

static const size_t STACK_SIZE = 1024 * 1024;
static const char* DEFAULT_ARGV0 = "id5.elf";

int main(int argc, char* argv[]) {

    // COM initialization
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Warning: CoInitializeEx failed: " << std::hex << hr << std::endl;
    }

    std::cout << "--- Windy Lindbergh Loader ---" << std::endl;

    if (argc < 2) {
        std::cerr << "Error: No ELF file path specified." << std::endl;
        std::cerr << "Usage: Windy.exe <path_to_elf>" << std::endl;
        system("pause");
        return 1;
    }

    ElfLoader loader(argv[1]);
    if (!loader.LoadToMemory()) {
        std::cerr << "Error: Failed to load ELF file into memory." << std::endl;
        system("pause");
        return 1;
    }

    // Perform dynamic linking (GOT patching)
    SymbolResolver::ResolveAll(
        loader.GetJmpRel(),
        loader.GetSymTab(),
        loader.GetStrTab(),
        loader.GetPltRelSize()
    );

    // Prepare Linux-style stack
    uint32_t finalEsp = LinuxStack::Setup(STACK_SIZE, DEFAULT_ARGV0);
    uint32_t entry = loader.GetHeader().e_entry;

    std::cout << "Jumping to Linux entry point: 0x" << std::hex << entry << std::endl;

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
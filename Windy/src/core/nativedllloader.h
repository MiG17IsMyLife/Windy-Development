#pragma once

#include "ilibraryloader.h"
#include <windows.h>
#include <string>
#include <vector>
#include "log.h"

class NativeDllLoader : public ILibraryLoader {
private:
    std::string linuxName;
    std::string winName;
    HMODULE hModule;
    std::vector<std::string> dependencies; // Usually empty for native DLLs as Windows handles imports

public:
    NativeDllLoader(const std::string& linuxLibName, const std::string& windowsDllName) 
        : linuxName(linuxLibName), winName(windowsDllName), hModule(NULL) {}

    ~NativeDllLoader() {
        if (hModule) {
            FreeLibrary(hModule);
        }
    }

    bool Load() {
        hModule = LoadLibraryA(winName.c_str());
        if (!hModule) {
            log_error("NativeDllLoader: Failed to load %s (WinErr: %d)", winName.c_str(), GetLastError());
            return false;
        }
        log_info("NativeDllLoader: Loaded %s (mapping for %s) at %p", winName.c_str(), linuxName.c_str(), hModule);
        return true;
    }

    uintptr_t GetSymbolAddress(const char* name) const override {
        if (!hModule) return 0;
        return (uintptr_t)GetProcAddress(hModule, name);
    }

    const std::vector<std::string>& GetDependencies() const override {
        return dependencies;
    }

    void ResolveSymbols() override {
        // Windows handles symbol resolution for native DLLs on LoadLibrary.
        log_debug("NativeDllLoader: ResolveSymbols no-op for %s", winName.c_str());
    }

    void RunInit() override {
        // Native DLLs run DllMain on LoadLibrary, so nothing explicit to do here.
        log_debug("NativeDllLoader: RunInit no-op for %s", winName.c_str());
    }

    void DebugPrintSymbols() const override {
        if (!hModule) return;

        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) return;

        PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDosHeader->e_lfanew);
        if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) return;

        DWORD exportDirRVA = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        if (exportDirRVA == 0) {
            log_info("NativeDllLoader: No export table found for %s", winName.c_str());
            return;
        }

        PIMAGE_EXPORT_DIRECTORY pExportDir = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hModule + exportDirRVA);
        
        DWORD* pNames = (DWORD*)((BYTE*)hModule + pExportDir->AddressOfNames);
        DWORD* pFuncs = (DWORD*)((BYTE*)hModule + pExportDir->AddressOfFunctions);
        WORD* pOrdinals = (WORD*)((BYTE*)hModule + pExportDir->AddressOfNameOrdinals);

        log_info("Exported Symbols for %s (Native):", winName.c_str());
        for (DWORD i = 0; i < pExportDir->NumberOfNames; i++) {
            const char* name = (const char*)((BYTE*)hModule + pNames[i]);
            WORD ordinal = pOrdinals[i];
            DWORD funcRVA = pFuncs[ordinal];
             
            // Check for forwarded exports if needed, but basic printing is enough
            log_info("  [NATIVE] %s -> 0x%p", name, (BYTE*)hModule + funcRVA);
        }
    }
};

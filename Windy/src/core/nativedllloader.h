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

    void RunInit() override {
        // Native DLLs run DllMain on LoadLibrary, so nothing explicit to do here.
        log_debug("NativeDllLoader: RunInit no-op for %s", winName.c_str());
    }

    void DebugPrintSymbols() const override {
        // Cannot list symbols from a DLL easily without DbgHelp
        log_info("NativeDllLoader: Symbols for %s are managed by Windows.", winName.c_str());
    }
};

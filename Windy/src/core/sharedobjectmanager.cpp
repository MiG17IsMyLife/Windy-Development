#include "sharedobjectmanager.h"
#include "elfloader.h"
#include "nativedllloader.h"
#include "log.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

ILibraryLoader* SharedObjectManager::LoadSharedLibrary(const std::string& name) {
    // 1. Check cache
    if (loadedLibraries.find(name) != loadedLibraries.end()) {
        return loadedLibraries[name];
    }

    // 2. Check Native Overrides
    if (nativeOverrides.find(name) != nativeOverrides.end()) {
        std::string winDll = nativeOverrides[name];
        log_info("SharedObjectManager: Redirecting %s -> %s (Native DLL)", name.c_str(), winDll.c_str());
        
        NativeDllLoader* dllLoader = new NativeDllLoader(name, winDll);
        if (dllLoader->Load()) {
            loadedLibraries[name] = dllLoader;
            return dllLoader;
        } else {
            log_error("SharedObjectManager: Failed to load native override %s", winDll.c_str());
            delete dllLoader;
            // Fallthrough? No, if override fails, we probably shouldn't try to load the ELF 
            // as it might be incompatible or mixed symbols. But maybe fallback is desired?
            // For now, fail hard on override failure to avoid confusion.
            return nullptr; 
        }
    }

    // 3. Spoofing Check (Libc redirection)
    // If it's a standard system library, we might want to skip loading it 
    // and rely on our bridges instead. 
    // OR we load a dummy ElfLoader that points to nothing but registers itself?
    // User requested "redirect libc calls to msys2", which implies we DON'T load libc.so
    // but when other libs ask for symbols from it, we return MSYS addresses.
    if (name.find("libc.so") != std::string::npos || 
        name.find("libm.so") != std::string::npos ||
        name.find("libpthread.so") != std::string::npos ||
        name.find("libdl.so") != std::string::npos ||
        name.find("libgcc_s.so") != std::string::npos ||
        name.find("ld-linux.so") != std::string::npos ||
        name.find("libstdc++.so") != std::string::npos) {
        log_info("Skipping load of %s (Redirecting to Native/MSYS)", name.c_str());
        // We register nullptr or a valid marker to say "handled natively"
        // But for GetGlobalSymbol, we need to know to fallback to SymbolResolver.
        loadedLibraries[name] = nullptr; 
        return nullptr;
    }

    // 4. Find File
    std::string fullPath = FindLibraryPath(name);
    if (fullPath.empty()) {
        log_error("Failed to find library: %s", name.c_str());
        return nullptr;
    }

    log_info("Loading shared library: %s", fullPath.c_str());

    // 5. Load
    ElfLoader* loader = new ElfLoader(fullPath.c_str());
    // Use 0 as base to let ReserveAddressSpace find a free spot
    if (!loader->LoadToMemory(0)) {
        log_error("Failed to load %s", name.c_str());
        delete loader;
        return nullptr;
    }
    
    loadedLibraries[name] = loader;

    // 6. Recursive Dependency Loading
    for (const auto& dep : loader->GetDependencies()) {
         log_info("  %s depends on %s", name.c_str(), dep.c_str());
         LoadSharedLibrary(dep);
    }
    
    // 7. Run Initialization
    // We run init AFTER dependencies are loaded (so they are ready)
    log_info("Running Initialization for %s", name.c_str());
    loader->RunInit();
    loader->DebugPrintSymbols();

    return loader;
}

uintptr_t SharedObjectManager::GetGlobalSymbol(const char* name) {
    // Search in all loaded ELF objects
    for (auto const& [libName, loader] : loadedLibraries) {
        if (loader) {
            uintptr_t addr = loader->GetSymbolAddress(name);
            if (addr != 0) return addr;
        }
    }
    return 0;
}

void SharedObjectManager::AddLoader(const std::string& name, ILibraryLoader* loader) {
    loadedLibraries[name] = loader;
}

void SharedObjectManager::AddSearchPath(const std::string& path) {
    searchPaths.push_back(path);
}

void SharedObjectManager::RegisterNativeOverride(const std::string& linuxName, const std::string& winName) {
    nativeOverrides[linuxName] = winName;
    log_info("Registered native override: %s -> %s", linuxName.c_str(), winName.c_str());
}

std::string SharedObjectManager::FindLibraryPath(const std::string& name) {
    // Check absolute path first
    if (std::filesystem::exists(name)) {
        return name;
    }
    
    for (const auto& path : searchPaths) {
        // Construct path: path + / + name
        // Use filesystem for safety
        std::filesystem::path p(path);
        p /= name;
        if (std::filesystem::exists(p)) {
            return p.string();
        }
    }
    return "";
}

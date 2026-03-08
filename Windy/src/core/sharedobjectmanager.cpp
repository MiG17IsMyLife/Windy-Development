#include "sharedobjectmanager.h"
#include "elfloader.h"
#include "nativedllloader.h"
#include "log.h"
#include <filesystem>

static bool ShouldSkipLibrary(const std::string& name) {
    static const char* skippedLibs[] = {
        "libc.so", "libm.so", "libpthread.so", "libdl.so",
        "libgcc_s.so", "ld-linux.so", "libkswapapi.so"
        // "libgcc_s.so", "ld-linux.so", "libsegaapi.so", "libkswapapi.so"
    };

    for (const char* lib : skippedLibs) {
        if (name.find(lib) != std::string::npos) {
            return true;
        }
    }
    return false;
}

ILibraryLoader* SharedObjectManager::LoadSharedLibrary(const std::string& name) {
    // 1. Resolve Path FIRST to check cache with canonical name
    std::string fullPath = FindLibraryPath(name);
    
    // If not found, check if it's a known skipped library before giving up
    if (fullPath.empty()) {
        // If it's one of these, we skip regardless of path presence
        if (ShouldSkipLibrary(name)) {
            log_info("Skipping load of %s (Redirecting to Native/MSYS)", name.c_str());
            loadedLibraries[name] = nullptr; 
            return nullptr;
        }

        // Native overrides might be registered with the short name
        if (nativeOverrides.find(name) != nativeOverrides.end()) {
             // Fall through to native override handler
             fullPath = name; // Just use name as key
        } else {
            log_error("Failed to find library: %s", name.c_str());
            return nullptr;
        }
    }

    // 2. Check Cache using FULL PATH (if available) or Name
    // We prefer using the full path as key to avoid double loading (e.g. libfoo.so vs /usr/lib/libfoo.so)
    // But we also need to map the requested name to this loader so subsequent lookups find it.
    std::string cacheKey = fullPath.empty() ? name : fullPath;

    if (loadedLibraries.find(cacheKey) != loadedLibraries.end()) {
        ILibraryLoader* loader = loadedLibraries[cacheKey];
        // Also register the requested name alias if it's different
        if (name != cacheKey) {
            loadedLibraries[name] = loader;
        }
        return loader;
    }

    // 3. Check Native Overrides
    if (nativeOverrides.find(name) != nativeOverrides.end()) {
        std::string winDll = nativeOverrides[name];
        log_info("SharedObjectManager: Redirecting %s -> %s (Native DLL)", name.c_str(), winDll.c_str());
        
        NativeDllLoader* dllLoader = new NativeDllLoader(name, winDll);
        if (dllLoader->Load()) {
            loadedLibraries[cacheKey] = dllLoader;
            if (name != cacheKey) loadedLibraries[name] = dllLoader;
            loadOrder.push_back(cacheKey);
            return dllLoader;
        } else {
            log_error("SharedObjectManager: Failed to load native override %s", winDll.c_str());
            delete dllLoader;
            return nullptr; 
        }
    }

    // 4. Spoofing Check (Libc redirection)
    // (Already handled partially above, but double check in case FindLibraryPath found a dummy file)
    if (ShouldSkipLibrary(name)) {
        log_info("Skipping load of %s (Redirecting to Native/MSYS)", name.c_str());
        loadedLibraries[name] = nullptr;
        if (name != cacheKey) loadedLibraries[cacheKey] = nullptr;
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
    
    loadedLibraries[cacheKey] = loader;
    if (name != cacheKey) loadedLibraries[name] = loader;
    loadOrder.push_back(cacheKey);

    // 6. Recursive Dependency Loading
    for (const auto& dep : loader->GetDependencies()) {
         log_info("  %s depends on %s", name.c_str(), dep.c_str());
         LoadSharedLibrary(dep);
    }
    
    // Initialization and Symbol Resolution are now handled in batch 
    // by SharedObjectManager::ResolveAllLibraries and RunAllInitializers.

    return loader;
}

void SharedObjectManager::ResolveAllLibraries() {
    log_info("SharedObjectManager: Resolving symbols for all loaded libraries...");
    log_info("  Load order (%zu libraries):", loadOrder.size());
    for (size_t i = 0; i < loadOrder.size(); ++i) {
        log_info("    [%zu] %s", i, loadOrder[i].c_str());
    }
    
    // Resolve shared libraries FIRST, before the main executable
    // This ensures vtables and other data in libstdc++ are properly relocated
    // before the main executable tries to copy them via R_386_COPY
    log_info("  Pass 1: Resolving shared libraries (reverse order)...");
    for (auto it = loadOrder.rbegin(); it != loadOrder.rend(); ++it) {
        const std::string& name = *it;
        
        // Skip main executable on first pass
        if (name == "main" || name.find(".exe") != std::string::npos) {
            log_info("    Skipping %s (will resolve in pass 2)", name.c_str());
            continue;
        }
        
        ILibraryLoader* loader = loadedLibraries[name];
        if (loader) {
            log_info("  Resolving symbols for %s", name.c_str());
            loader->ResolveSymbols();
        }
    }
    
    // Then resolve the main executable
    log_info("  Pass 2: Resolving main executable...");
    for (const auto& name : loadOrder) {
        if (name == "main" || name.find(".exe") != std::string::npos) {
            log_info("  Resolving symbols for %s", name.c_str());
            loadedLibraries[name]->ResolveSymbols();
            break;
        }
    }
}

void SharedObjectManager::RunAllInitializers() {
    log_info("SharedObjectManager: Running initializers for all loaded libraries...");
    // Initializers should be run in reverse load order (dependencies first)
    // Actually, our recursive loading adds them in the order they are encountered.
    // To run dependencies first, we should use reverse post-order.
    // However, given the current recursive structure, loadOrder is roughly dependency order. 
    // In Linux, initializers are run such that a library's dependencies are initialized before itself.
    
    for (auto it = loadOrder.rbegin(); it != loadOrder.rend(); ++it) {
        const std::string& name = *it;

        // Skip the main ELF — its _start/entry point handles its own
        // initialization via __libc_start_main.  Running DT_INIT /
        // INIT_ARRAY here would invoke Linux-specific init code
        // (frame_dummy, __libc_csu_init, etc.) that hangs on Windows.
        if (name == "main") {
            log_info("  Skipping RunInit for main ELF (handled by entry point)");
            if (loadedLibraries[name])
                loadedLibraries[name]->DebugPrintSymbols();
            continue;
        }

        if (loadedLibraries[name]) {
            log_info("  Initializing %s", name.c_str());
            loadedLibraries[name]->RunInit();
            loadedLibraries[name]->DebugPrintSymbols();
        }
    }
}

uintptr_t SharedObjectManager::GetGlobalSymbol(const char* name) {
    // Check if this is a vtable symbol for logging
    bool isVtable = (strncmp(name, "_ZTV", 4) == 0 || strncmp(name, "_ZTT", 4) == 0);
    
    if (isVtable) {
        log_info("SharedObjectManager: Searching for VTABLE '%s' in %zu loaded libraries", 
                 name, loadOrder.size());
    }
    
    // Search in all loaded ELF objects IN LOAD ORDER
    for (const auto& libName : loadOrder) {
        ILibraryLoader* loader = loadedLibraries[libName];
        if (loader) {
            uintptr_t addr = loader->GetSymbolAddress(name);
            if (addr != 0) {
                // Check if this is a vtable symbol for logging
                if (isVtable) {
                    log_info("SharedObjectManager: VTABLE symbol '%s' found in %s at 0x%p", name, libName.c_str(), (void*)addr);
                }
                return addr;
            }
        }
    }
    
    // Log if searching for vtable symbols
    if (isVtable) {
        log_error("SharedObjectManager: VTABLE symbol '%s' NOT FOUND in any library", name);
        log_error("  Loaded libraries (%zu):", loadOrder.size());
        for (const auto& libName : loadOrder) {
            log_error("    - %s", libName.c_str());
        }
    }
    
    return 0;
}

uintptr_t SharedObjectManager::GetGlobalSymbolExcludingMain(const char* name) {
    // Search in all loaded ELF objects EXCLUDING the main executable
    // This is used for R_386_COPY relocations which copy from shared libs to executable
    for (const auto& libName : loadOrder) {
        // Skip the main executable
        if (libName == "main" || libName.find(".exe") != std::string::npos) {
            continue;
        }
        
        ILibraryLoader* loader = loadedLibraries[libName];
        if (loader) {
            uintptr_t addr = loader->GetSymbolAddress(name);
            if (addr != 0) {
                log_debug("SharedObjectManager: Symbol '%s' found in %s (excluding main) at 0x%p", 
                          name, libName.c_str(), (void*)addr);
                return addr;
            }
        }
    }
    
    return 0;
}

void SharedObjectManager::AddLoader(const std::string& name, ILibraryLoader* loader) {
    loadedLibraries[name] = loader;
    loadOrder.push_back(name);
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

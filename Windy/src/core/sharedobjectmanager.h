#pragma once
#include "ilibraryloader.h"
#include <string>
#include <map>
#include <vector>

class SharedObjectManager {
public:
    static SharedObjectManager& Instance() {
        static SharedObjectManager instance;
        return instance;
    }

    // Load a library by name (recursive dependency loading)
    // Returns cached loader if already loaded.
    ILibraryLoader* LoadSharedLibrary(const std::string& name);

    // Global symbol lookup across all loaded libraries
    uintptr_t GetGlobalSymbol(const char* name);

    // Add a pre-loaded loader (e.g. the main executable)
    void AddLoader(const std::string& name, ILibraryLoader* loader);
    
    // Add paths to search for libs
    void AddSearchPath(const std::string& path);

    // Registry for overrides
    void RegisterNativeOverride(const std::string& linuxName, const std::string& winName);

private:
    SharedObjectManager() {}
    
    std::string FindLibraryPath(const std::string& name);

    std::map<std::string, ILibraryLoader*> loadedLibraries;
    std::vector<std::string> searchPaths;
    std::map<std::string, std::string> nativeOverrides;
};

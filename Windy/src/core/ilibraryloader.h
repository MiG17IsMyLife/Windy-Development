#pragma once

#include <string>
#include <vector>
#include <stdint.h>

class ILibraryLoader {
public:
    virtual ~ILibraryLoader() = default;

    // Core symbol lookup
    virtual uintptr_t GetSymbolAddress(const char* name) const = 0;

    // Dependency management
    virtual const std::vector<std::string>& GetDependencies() const = 0;

    // Initialization (constructors etc)
    virtual void RunInit() = 0;

    // Symbol Resolution (GOT/PLT)
    virtual void ResolveSymbols() = 0;

    // Debugging
    virtual void DebugPrintSymbols() const {}
};

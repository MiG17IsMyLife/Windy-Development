#pragma once

#include <cstdint>
#include <stdint.h>

class Patches
{
  public:
    static void Apply(uint8_t gameId);
    static void PatchMemoryFromString(uintptr_t address, const char *value);
    static void SetVariable(uintptr_t address, uint32_t value);
};
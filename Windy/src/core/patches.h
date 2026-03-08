#pragma once

#include <cstdint>
#include <stdint.h>

class Patches
{
  public:
    static void Apply(uint8_t gameId);
    static void patchMemoryFromString(uintptr_t address, const char *value);
};

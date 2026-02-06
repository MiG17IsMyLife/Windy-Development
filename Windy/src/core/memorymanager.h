#pragma once
#include "common.h"

class MemoryManager
{
  public:
    static LPVOID ReserveAddressSpace(uintptr_t minAddr, uintptr_t maxAddr, uintptr_t &reservedBase, uintptr_t &reservedSize);
    static bool CommitSegment(uintptr_t vaddr, uintptr_t memsz);
};
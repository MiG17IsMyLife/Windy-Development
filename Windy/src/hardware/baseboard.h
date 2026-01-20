#pragma once
#include "LindberghDevice.h"
#include <stdint.h>
#include <stdio.h>

class BaseBoard : public LindberghDevice {
private:
    // 32KB Shared Memory
    uint8_t m_sharedMemory[1024 * 32];
    unsigned int m_sharedMemoryIndex = 0;

    // Persistent File Handle for SRAM
    FILE* m_sramFile = nullptr;
    const char* m_sramPath = "sram.bin";

    // Command State
    struct {
        uint32_t srcAddress;
        uint32_t srcSize;
        uint32_t destAddress;
        uint32_t destSize;
    } m_jvsCommand, m_serialCommand;

    // Buffer for JVS input emulation
    uint8_t m_inputBuffer[1024];

public:
    BaseBoard();
    virtual ~BaseBoard();

    bool Open() override;
    void Close() override;
    int Read(void* buf, size_t count) override;
    int Write(const void* buf, size_t count) override;
    int Ioctl(unsigned long request, void* data) override;
};
#pragma once
#include "lindberghdevice.h"
#include "JvsBoard.h"
#include <vector>

class BaseBoard : public LindberghDevice {
private:
    std::vector<uint8_t> sharedMemory;
    std::vector<uint8_t> sram;
    JvsBoard jvs;

    struct {
        uint32_t srcAddress;
        uint32_t srcSize;
        uint32_t destAddress;
        uint32_t destSize;
    } jvsCommand, serialCommand;

    struct readData_t {
        uint32_t* data;
        uint32_t offset;
        uint32_t size;
    };

    struct writeData_t {
        uint32_t offset;
        uint32_t* data;
        uint32_t size;
    };

    unsigned int sharedMemoryIndex = 0;

public:
    BaseBoard();
    int Ioctl(unsigned long request, void* data) override;
    int Read(void* buf, size_t count) override;
    int Write(const void* buf, size_t count) override;
};
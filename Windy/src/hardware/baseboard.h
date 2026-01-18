#pragma once
#include "LindberghDevice.h"

// TODO: Add baseboard implementions from original Lindbergh-Loader
class BaseBoard : public LindberghDevice {
public:
    BaseBoard() {}
    virtual ~BaseBoard() {}

    bool Open() override;
    void Close() override;
    int Read(void* buf, size_t count) override;
    int Write(const void* buf, size_t count) override;
    int Ioctl(unsigned long request, void* data) override;
};
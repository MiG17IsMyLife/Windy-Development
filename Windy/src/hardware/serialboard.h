#pragma once
#include "LindberghDevice.h"

class SerialBoard : public LindberghDevice {
public:
    SerialBoard();
    virtual ~SerialBoard();

    bool Open() override;
    void Close() override;
    int Read(void* buf, size_t count) override;
    int Write(const void* buf, size_t count) override;
    int Ioctl(unsigned long request, void* data) override;
};
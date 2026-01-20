#pragma once
#include "LindberghDevice.h"
#include <stdio.h>
#include <stdint.h>

class EepromBoard : public LindberghDevice {
private:
    FILE* m_file = nullptr;
    const char* m_path = "eeprom.bin";

public:
    EepromBoard();
    virtual ~EepromBoard();

    bool Open() override;
    void Close() override;
    int Ioctl(unsigned long request, void* data) override;

    // Read/Write ‚Í’ÊíI2C‚Å‚ÍioctlŒo—R‚¾‚ªA”O‚Ì‚½‚ß
    int Read(void* buf, size_t count) override { return 0; }
    int Write(const void* buf, size_t count) override { return 0; }
};
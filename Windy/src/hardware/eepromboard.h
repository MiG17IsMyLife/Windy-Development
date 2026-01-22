#pragma once
#include "LindberghDevice.h"
#include <stdint.h>

#define EEPROM_SIZE 2048  // 2KB EEPROM

class EepromBoard : public LindberghDevice {
private:
    uint8_t m_eepromData[EEPROM_SIZE];
    uint8_t m_slaveAddress;
    uint16_t m_currentOffset;

    // Helper methods
    uint8_t ReadByte(uint16_t offset);
    void WriteByte(uint16_t offset, uint8_t value);
    uint16_t ReadWord(uint16_t offset);
    void WriteWord(uint16_t offset, uint16_t value);

public:
    EepromBoard();
    virtual ~EepromBoard();

    bool Open() override;
    void Close() override;
    int Read(void* buf, size_t count) override;
    int Write(const void* buf, size_t count) override;
    int Ioctl(unsigned long request, void* data) override;
};
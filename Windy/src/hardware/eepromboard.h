#pragma once
#include "LindberghDevice.h"
#include <stdint.h>
#include <stdio.h>

#define EEPROM_SIZE 1024
#define EEPROM_FILENAME "eeprom.bin"
#define EEPROM_ADDR 0x50

class EepromBoard : public LindberghDevice {
private:
    uint8_t m_data[EEPROM_SIZE];
    uint16_t m_addressPointer;
    uint8_t m_slaveAddress;

    void Load();
    void Save();
    void InitDefaults();
    uint32_t CalculateCRC(const uint8_t* data, size_t length);

public:
    EepromBoard();
    virtual ~EepromBoard();

    bool Open() override;
    void Close() override;
    int Read(void* buf, size_t count) override;
    int Write(const void* buf, size_t count) override;
    int Ioctl(unsigned long request, void* data) override;
};
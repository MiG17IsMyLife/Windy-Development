#define _CRT_SECURE_NO_WARNINGS
#include "EepromBoard.h"
#include <stdio.h>
#include <string.h>

// I2C Definitions
#define I2C_SLAVE           0x703
#define I2C_GET_FUNCTIONS   0x705
#define I2C_SMBUS           0x720

// SMBus Transaction Types (from lindbergh-loader)
#define I2C_READ    1
#define I2C_SEEK    2
#define I2C_WRITE   3

#define I2C_SMBUS_BLOCK_MAX 32

union i2c_smbus_data {
    uint8_t byte;
    uint16_t word;
    uint8_t block[I2C_SMBUS_BLOCK_MAX + 2];
};

struct i2c_smbus_ioctl_data {
    uint8_t read_write;
    uint8_t command;
    uint32_t size;
    union i2c_smbus_data* data;
};

EepromBoard::EepromBoard() {}

EepromBoard::~EepromBoard() {
    Close();
}

bool EepromBoard::Open() {
    printf("[EEPROM] Open\n");
    // Ensure file exists
    m_file = fopen(m_path, "a");
    if (m_file) fclose(m_file);

    m_file = fopen(m_path, "rb+");
    if (!m_file) {
        printf("[EEPROM] Failed to open %s\n", m_path);
        return false;
    }
    return true;
}

void EepromBoard::Close() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}

int EepromBoard::Ioctl(unsigned long request, void* data) {
    switch (request) {

    case I2C_SLAVE:
        // Set Slave Address (Dummy)
        return 0;

    case I2C_GET_FUNCTIONS:
        if (data) {
            uint32_t* func = (uint32_t*)data;
            // Capability bits from lindbergh-loader
            *func = 0x20000 | 0x40000 | 0x100000 | 0x400000 | 0x8000000;
            *func |= 0x80000 | 0x200000 | 0x1000000 | 0x2000000;
        }
        return 0;

    case I2C_SMBUS:
        if (data && m_file) {
            struct i2c_smbus_ioctl_data* d = (struct i2c_smbus_ioctl_data*)data;

            switch (d->size) {
            case I2C_READ:
                // 1 byte read
                fread(&d->data->byte, 1, 1, m_file);
                break;

            case I2C_SEEK:
            {
                // Address calculation: (Command << 8) | Data
                uint16_t addr = (d->command & 0xFF) << 8 | (d->data->byte & 0xFF);
                fseek(m_file, addr, SEEK_SET);
            }
            break;

            case I2C_WRITE:
            {
                uint16_t addr = (d->command & 0xFF) << 8 | (d->data->byte & 0xFF);
                uint8_t val = (d->data->word >> 8) & 0xFF;

                fseek(m_file, addr, SEEK_SET);
                fwrite(&val, 1, 1, m_file);
                fflush(m_file);
            }
            break;

            default:
                printf("[EEPROM] Unknown SMBus size: %d\n", d->size);
                break;
            }
        }
        return 0;

    default:
        printf("[EEPROM] Unknown Ioctl: 0x%lX\n", request);
        break;
    }
    return -1;
}
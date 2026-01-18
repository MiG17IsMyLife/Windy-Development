#include "EepromBoard.h"
#include <iostream>

#define I2C_GET_FUNCTIONS 0x705
#define I2C_SMBUS_TRANSFER 0x720
#define I2C_SET_SLAVE_MODE 0x703
#define I2C_BUFFER_CLEAR 0x1261
#define I2C_READ 1
#define I2C_SEEK 2
#define I2C_WRITE 3

union i2c_smbus_data {
    uint8_t byte;
    uint16_t word;
    uint8_t block[34];
};

struct i2c_smbus_ioctl_data {
    uint8_t read_write;
    uint8_t command;
    uint32_t size;
    union i2c_smbus_data* data;
};

int EepromBoard::Ioctl(unsigned long request, void* data) {
    switch (request) {
    case I2C_GET_FUNCTIONS: {
        uint32_t* funcs = (uint32_t*)data;
        *funcs = 0x20000 | 0x40000 | 0x80000 | 0x100000 | 0x200000 | 0x400000 | 0x1000000 | 0x2000000 | 0x8000000;
        return 0;
    }
    case I2C_SMBUS_TRANSFER: {
        struct i2c_smbus_ioctl_data* d = (struct i2c_smbus_ioctl_data*)data;
        if (d->read_write == I2C_READ && d->data) {
            // Return valid data to prevent loops. Returning 0 is safe for now.
            d->data->byte = 0;
        }
        return 0;
    }
    case I2C_SET_SLAVE_MODE:
    case I2C_BUFFER_CLEAR:
        return 0;
    }
    return -1;
}
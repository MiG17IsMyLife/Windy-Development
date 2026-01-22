#include "EepromBoard.h"
#include "../src/core/log.h"
#include <cstring>

// I2C IOCTL definitions
#define I2C_GET_FUNCTIONS   0x705
#define I2C_SMBUS_TRANSFER  0x720
#define I2C_SET_SLAVE_MODE  0x703
#define I2C_BUFFER_CLEAR    0x1261

// I2C operation types
#define I2C_SMBUS_READ  1
#define I2C_SMBUS_WRITE 0

// I2C SMBUS transaction types
#define I2C_SMBUS_BYTE          1
#define I2C_SMBUS_BYTE_DATA     2
#define I2C_SMBUS_WORD_DATA     3
#define I2C_SMBUS_BLOCK_DATA    5

// I2C function flags
#define I2C_FUNC_SMBUS_READ_BYTE        0x00020000
#define I2C_FUNC_SMBUS_WRITE_BYTE       0x00040000
#define I2C_FUNC_SMBUS_READ_BYTE_DATA   0x00080000
#define I2C_FUNC_SMBUS_WRITE_BYTE_DATA  0x00100000
#define I2C_FUNC_SMBUS_READ_WORD_DATA   0x00200000
#define I2C_FUNC_SMBUS_WRITE_WORD_DATA  0x00400000
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA  0x01000000
#define I2C_FUNC_SMBUS_WRITE_BLOCK_DATA 0x02000000

union i2c_smbus_data {
    uint8_t byte;
    uint16_t word;
    uint8_t block[34];  // block[0] is length, block[1..32] is data
};

struct i2c_smbus_ioctl_data {
    uint8_t read_write;     // I2C_SMBUS_READ or I2C_SMBUS_WRITE
    uint8_t command;        // Register/command byte
    uint32_t size;          // Transaction type
    union i2c_smbus_data* data;
};

EepromBoard::EepromBoard() {
    memset(m_eepromData, 0, sizeof(m_eepromData));
    m_slaveAddress = 0;
    m_currentOffset = 0;
}

EepromBoard::~EepromBoard() {
    Close();
}

bool EepromBoard::Open() {
    log_info("EepromBoard::Open()");

    // Initialize EEPROM with default data if needed
    // This could be loaded from a file in the future

    return true;
}

void EepromBoard::Close() {
    log_debug("EepromBoard::Close()");
}

int EepromBoard::Read(void* buf, size_t count) {
    if (!buf || count == 0) return -1;

    size_t bytesToRead = count;
    if (m_currentOffset + bytesToRead > sizeof(m_eepromData)) {
        bytesToRead = sizeof(m_eepromData) - m_currentOffset;
    }

    memcpy(buf, &m_eepromData[m_currentOffset], bytesToRead);
    m_currentOffset += bytesToRead;

    log_trace("EepromBoard::Read(%zu bytes from offset %zu)", bytesToRead, m_currentOffset - bytesToRead);
    return (int)bytesToRead;
}

int EepromBoard::Write(const void* buf, size_t count) {
    if (!buf || count == 0) return -1;

    size_t bytesToWrite = count;
    if (m_currentOffset + bytesToWrite > sizeof(m_eepromData)) {
        bytesToWrite = sizeof(m_eepromData) - m_currentOffset;
    }

    memcpy(&m_eepromData[m_currentOffset], buf, bytesToWrite);
    m_currentOffset += bytesToWrite;

    log_trace("EepromBoard::Write(%zu bytes at offset %zu)", bytesToWrite, m_currentOffset - bytesToWrite);
    return (int)bytesToWrite;
}

int EepromBoard::Ioctl(unsigned long request, void* data) {
    switch (request) {

    case I2C_GET_FUNCTIONS: {
        if (!data) return -1;

        uint32_t* funcs = (uint32_t*)data;
        *funcs = I2C_FUNC_SMBUS_READ_BYTE |
            I2C_FUNC_SMBUS_WRITE_BYTE |
            I2C_FUNC_SMBUS_READ_BYTE_DATA |
            I2C_FUNC_SMBUS_WRITE_BYTE_DATA |
            I2C_FUNC_SMBUS_READ_WORD_DATA |
            I2C_FUNC_SMBUS_WRITE_WORD_DATA |
            I2C_FUNC_SMBUS_READ_BLOCK_DATA |
            I2C_FUNC_SMBUS_WRITE_BLOCK_DATA;

        log_debug("EepromBoard: I2C_GET_FUNCTIONS -> 0x%08X", *funcs);
        return 0;
    }

    case I2C_SET_SLAVE_MODE: {
        // data is the slave address (cast from pointer)
        m_slaveAddress = (uint8_t)(uintptr_t)data;
        log_debug("EepromBoard: I2C_SET_SLAVE_MODE -> 0x%02X", m_slaveAddress);
        return 0;
    }

    case I2C_BUFFER_CLEAR: {
        m_currentOffset = 0;
        log_trace("EepromBoard: I2C_BUFFER_CLEAR");
        return 0;
    }

    case I2C_SMBUS_TRANSFER: {
        if (!data) return -1;

        struct i2c_smbus_ioctl_data* smbus = (struct i2c_smbus_ioctl_data*)data;

        if (smbus->read_write == I2C_SMBUS_READ) {
            // Read operation
            switch (smbus->size) {
            case I2C_SMBUS_BYTE:
                if (smbus->data) {
                    smbus->data->byte = ReadByte(m_currentOffset++);
                    log_trace("EepromBoard: SMBUS_READ_BYTE -> 0x%02X", smbus->data->byte);
                }
                break;

            case I2C_SMBUS_BYTE_DATA:
                if (smbus->data) {
                    smbus->data->byte = ReadByte(smbus->command);
                    log_trace("EepromBoard: SMBUS_READ_BYTE_DATA[0x%02X] -> 0x%02X",
                        smbus->command, smbus->data->byte);
                }
                break;

            case I2C_SMBUS_WORD_DATA:
                if (smbus->data) {
                    smbus->data->word = ReadWord(smbus->command);
                    log_trace("EepromBoard: SMBUS_READ_WORD_DATA[0x%02X] -> 0x%04X",
                        smbus->command, smbus->data->word);
                }
                break;

            case I2C_SMBUS_BLOCK_DATA:
                if (smbus->data) {
                    uint8_t len = ReadByte(smbus->command);
                    smbus->data->block[0] = len;
                    for (int i = 0; i < len && i < 32; i++) {
                        smbus->data->block[i + 1] = ReadByte(smbus->command + 1 + i);
                    }
                    log_trace("EepromBoard: SMBUS_READ_BLOCK_DATA[0x%02X] len=%d",
                        smbus->command, len);
                }
                break;

            default:
                log_warn("EepromBoard: Unknown SMBUS read size %d", smbus->size);
                if (smbus->data) smbus->data->byte = 0;
                break;
            }
        }
        else {
            // Write operation
            switch (smbus->size) {
            case I2C_SMBUS_BYTE:
                WriteByte(m_currentOffset++, smbus->command);
                log_trace("EepromBoard: SMBUS_WRITE_BYTE 0x%02X", smbus->command);
                break;

            case I2C_SMBUS_BYTE_DATA:
                if (smbus->data) {
                    WriteByte(smbus->command, smbus->data->byte);
                    log_trace("EepromBoard: SMBUS_WRITE_BYTE_DATA[0x%02X] = 0x%02X",
                        smbus->command, smbus->data->byte);
                }
                break;

            case I2C_SMBUS_WORD_DATA:
                if (smbus->data) {
                    WriteWord(smbus->command, smbus->data->word);
                    log_trace("EepromBoard: SMBUS_WRITE_WORD_DATA[0x%02X] = 0x%04X",
                        smbus->command, smbus->data->word);
                }
                break;

            case I2C_SMBUS_BLOCK_DATA:
                if (smbus->data) {
                    uint8_t len = smbus->data->block[0];
                    for (int i = 0; i < len && i < 32; i++) {
                        WriteByte(smbus->command + i, smbus->data->block[i + 1]);
                    }
                    log_trace("EepromBoard: SMBUS_WRITE_BLOCK_DATA[0x%02X] len=%d",
                        smbus->command, len);
                }
                break;

            default:
                log_warn("EepromBoard: Unknown SMBUS write size %d", smbus->size);
                break;
            }
        }
        return 0;
    }

    default:
        log_warn("EepromBoard: Unknown ioctl 0x%lX", request);
        return -1;
    }
}

// Private helper methods
uint8_t EepromBoard::ReadByte(uint16_t offset) {
    if (offset >= sizeof(m_eepromData)) {
        log_warn("EepromBoard: ReadByte offset 0x%04X out of range", offset);
        return 0;
    }
    return m_eepromData[offset];
}

void EepromBoard::WriteByte(uint16_t offset, uint8_t value) {
    if (offset >= sizeof(m_eepromData)) {
        log_warn("EepromBoard: WriteByte offset 0x%04X out of range", offset);
        return;
    }
    m_eepromData[offset] = value;
}

uint16_t EepromBoard::ReadWord(uint16_t offset) {
    if (offset + 1 >= sizeof(m_eepromData)) {
        log_warn("EepromBoard: ReadWord offset 0x%04X out of range", offset);
        return 0;
    }
    return m_eepromData[offset] | (m_eepromData[offset + 1] << 8);
}

void EepromBoard::WriteWord(uint16_t offset, uint16_t value) {
    if (offset + 1 >= sizeof(m_eepromData)) {
        log_warn("EepromBoard: WriteWord offset 0x%04X out of range", offset);
        return;
    }
    m_eepromData[offset] = value & 0xFF;
    m_eepromData[offset + 1] = (value >> 8) & 0xFF;
}
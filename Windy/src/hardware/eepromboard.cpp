#define _CRT_SECURE_NO_WARNINGS
#include "EepromBoard.h"
#include "../src/core/log.h"
#include <cstring>
#include <stdio.h>
#include <io.h>

// =============================================================
//   I2C Definitions
// =============================================================

#define I2C_SMBUS_READ  1
#define I2C_SMBUS_WRITE 0

#define I2C_SMBUS_BYTE              1
#define I2C_SMBUS_BYTE_DATA         2
#define I2C_SMBUS_WORD_DATA         3
#define I2C_SMBUS_BLOCK_DATA        5
#define I2C_SMBUS_I2C_BLOCK_BROKEN  6
#define I2C_SMBUS_I2C_BLOCK_DATA    8

#define I2C_SLAVE         0x0703
#define I2C_FUNCS         0x0705
#define I2C_SMBUS         0x0720
#define I2C_BUFFER_CLEAR  0x1261

// I2C SMBUS Data Union
union i2c_smbus_data {
    uint8_t byte;
    uint16_t word;
    uint8_t block[34];  // block[0] = length, block[1...] = data
};

// I2C SMBUS IOCTL Structure
struct i2c_smbus_ioctl_data {
    uint8_t read_write;
    uint8_t command;
    uint32_t size;
    union i2c_smbus_data* data;
};

// =============================================================
//   CRC32 Table (Standard)
// =============================================================

static const uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

// =============================================================
//   EepromBoard Implementation
// =============================================================

uint32_t EepromBoard::CalculateCRC(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crc32_tab[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

EepromBoard::EepromBoard() {
    memset(m_data, 0, EEPROM_SIZE);
    m_addressPointer = 0;
    m_slaveAddress = 0;
}

EepromBoard::~EepromBoard() {
    Close();
}

void EepromBoard::InitDefaults() {
    log_info("EepromBoard: Initializing default settings...");
    memset(m_data, 0, EEPROM_SIZE);

    // Save empty EEPROM - game will initialize it
    Save();
}

bool EepromBoard::Open() {
    log_info("EepromBoard: Open");

    // Check if file exists
    if (_access(EEPROM_FILENAME, 0) != 0) {
        log_warn("EepromBoard: %s not found. Creating new.", EEPROM_FILENAME);
        InitDefaults();
    }
    else {
        Load();
    }
    return true;
}

void EepromBoard::Close() {
    Save();
}

void EepromBoard::Load() {
    FILE* f = fopen(EEPROM_FILENAME, "rb");
    if (f) {
        size_t bytesRead = fread(m_data, 1, EEPROM_SIZE, f);
        fclose(f);
        log_info("EepromBoard: Loaded %s (%zu bytes)", EEPROM_FILENAME, bytesRead);
    }
    else {
        log_error("EepromBoard: Load failed for %s", EEPROM_FILENAME);
    }
}

void EepromBoard::Save() {
    log_info(">>> EepromBoard::Save() ENTRY");
    FILE* f = fopen(EEPROM_FILENAME, "wb");
    if (f) {
        fwrite(m_data, 1, EEPROM_SIZE, f);
        fclose(f);
        log_info(">>> EepromBoard::Save() EXIT - Success");
    }
    else {
        log_error("EepromBoard: Save failed for %s", EEPROM_FILENAME);
        log_info(">>> EepromBoard::Save() EXIT - Failed");
    }
}

int EepromBoard::Read(void* buf, size_t count) {
    // Direct read - read from current address pointer
    if (count > EEPROM_SIZE) count = EEPROM_SIZE;

    size_t bytesRead = 0;
    uint8_t* dst = (uint8_t*)buf;

    for (size_t i = 0; i < count; i++) {
        dst[i] = m_data[m_addressPointer];
        m_addressPointer = (m_addressPointer + 1) % EEPROM_SIZE;
        bytesRead++;
    }

    return (int)bytesRead;
}

int EepromBoard::Write(const void* buf, size_t count) {
    // Direct write - write to current address pointer
    if (count > EEPROM_SIZE) count = EEPROM_SIZE;

    const uint8_t* src = (const uint8_t*)buf;

    for (size_t i = 0; i < count; i++) {
        m_data[m_addressPointer] = src[i];
        m_addressPointer = (m_addressPointer + 1) % EEPROM_SIZE;
    }

    Save();
    return (int)count;
}

int EepromBoard::Ioctl(unsigned long request, void* data) {
    log_info(">>> EepromBoard::Ioctl ENTRY: request=0x%lX", request);

    switch (request) {

        // ---------------------------------------------------------
        // I2C_SLAVE: Set slave address
        // ---------------------------------------------------------
    case I2C_SLAVE:
        m_slaveAddress = (uint8_t)(uintptr_t)data;
        log_debug("EepromBoard: I2C_SLAVE set to 0x%02X", m_slaveAddress);
        log_info(">>> EepromBoard::Ioctl EXIT: I2C_SLAVE returning 0");
        return 0;

        // ---------------------------------------------------------
        // I2C_FUNCS: Report supported functionality
        // ---------------------------------------------------------
    case I2C_FUNCS:
        if (data) {
            *(uint32_t*)data = 0xFFFFFFFF; // Support everything
        }
        log_info(">>> EepromBoard::Ioctl EXIT: I2C_FUNCS returning 0");
        return 0;

        // ---------------------------------------------------------
        // I2C_BUFFER_CLEAR: Lindbergh-specific buffer clear
        // ---------------------------------------------------------
    case I2C_BUFFER_CLEAR:
        log_trace("EepromBoard: I2C_BUFFER_CLEAR (0x1261) - OK");
        log_info(">>> EepromBoard::Ioctl EXIT: I2C_BUFFER_CLEAR returning 0");
        return 0;

        // ---------------------------------------------------------
        // I2C_SMBUS: SMBus protocol operations
        // ---------------------------------------------------------
    case I2C_SMBUS:
    {
        log_info(">>> EepromBoard::Ioctl: I2C_SMBUS processing START");

        if (!data) {
            log_info(">>> EepromBoard::Ioctl EXIT: I2C_SMBUS null data returning -1");
            return -1;
        }
        struct i2c_smbus_ioctl_data* smbus = (struct i2c_smbus_ioctl_data*)data;

        log_info(">>> SMBUS: read_write=%d, command=0x%02X, size=%d",
            smbus->read_write, smbus->command, smbus->size);

        // Check target address - accept both 0x50 and 0x57
        uint8_t targetAddr = m_slaveAddress;
        if (targetAddr != EEPROM_ADDR && targetAddr != EEPROM_ADDR_ALT) {
            log_trace("SMBUS: Ignoring non-EEPROM address 0x%02X", targetAddr);
            log_info(">>> EepromBoard::Ioctl EXIT: I2C_SMBUS ignored addr returning 0");
            return 0;
        }

        // =====================================================
        // READ Operations
        // =====================================================
        if (smbus->read_write == I2C_SMBUS_READ) {
            log_info(">>> SMBUS READ: size=%d", smbus->size);

            switch (smbus->size) {

            case I2C_SMBUS_BYTE:
            {
                // Read single byte from current address pointer
                smbus->data->byte = m_data[m_addressPointer % EEPROM_SIZE];
                log_trace("SMBUS READ BYTE: addr=0x%04X -> 0x%02X",
                    m_addressPointer, smbus->data->byte);
                m_addressPointer = (m_addressPointer + 1) % EEPROM_SIZE;
                log_info(">>> EepromBoard::Ioctl EXIT: SMBUS READ BYTE returning 0");
                return 0;
            }

            case I2C_SMBUS_BYTE_DATA:
            {
                // Read single byte from specified register
                uint16_t addr = smbus->command;
                smbus->data->byte = m_data[addr % EEPROM_SIZE];
                log_trace("SMBUS READ BYTE_DATA: reg=0x%02X -> 0x%02X",
                    smbus->command, smbus->data->byte);
                log_info(">>> EepromBoard::Ioctl EXIT: SMBUS READ BYTE_DATA returning 0");
                return 0;
            }

            case I2C_SMBUS_WORD_DATA:
            {
                // Read word (2 bytes) from specified register
                uint16_t addr = smbus->command;
                smbus->data->word = m_data[addr % EEPROM_SIZE] |
                    (m_data[(addr + 1) % EEPROM_SIZE] << 8);
                log_trace("SMBUS READ WORD_DATA: reg=0x%02X -> 0x%04X",
                    smbus->command, smbus->data->word);
                log_info(">>> EepromBoard::Ioctl EXIT: SMBUS READ WORD_DATA returning 0");
                return 0;
            }

            case I2C_SMBUS_BLOCK_DATA:
            case I2C_SMBUS_I2C_BLOCK_BROKEN:
            case I2C_SMBUS_I2C_BLOCK_DATA:
            {
                // Read block of data
                uint8_t len = smbus->data->block[0];
                if (len > 32) len = 32;

                uint16_t baseAddr = smbus->command;

                for (int i = 0; i < len; i++) {
                    smbus->data->block[i + 1] = m_data[(baseAddr + i) % EEPROM_SIZE];
                }
                smbus->data->block[0] = len;

                log_trace("SMBUS READ BLOCK: reg=0x%02X len=%d -> [0x%02X 0x%02X 0x%02X ...]",
                    smbus->command, len,
                    len > 0 ? smbus->data->block[1] : 0,
                    len > 1 ? smbus->data->block[2] : 0,
                    len > 2 ? smbus->data->block[3] : 0);
                log_info(">>> EepromBoard::Ioctl EXIT: SMBUS READ BLOCK returning 0");
                return 0;
            }

            default:
                log_warn("SMBUS READ: Unknown size %d", smbus->size);
                log_info(">>> EepromBoard::Ioctl EXIT: SMBUS READ unknown size returning 0");
                return 0;
            }
        }
        // =====================================================
        // WRITE Operations
        // =====================================================
        else {
            log_info(">>> SMBUS WRITE: size=%d", smbus->size);

            switch (smbus->size) {

            case I2C_SMBUS_BYTE:
            {
                // Set address pointer (used before sequential reads)
                m_addressPointer = smbus->command;
                log_trace("SMBUS WRITE BYTE: Set address pointer to 0x%02X", smbus->command);
                log_info(">>> EepromBoard::Ioctl EXIT: SMBUS WRITE BYTE returning 0");
                return 0;
            }

            case I2C_SMBUS_BYTE_DATA:
            {
                // Write single byte to specified register
                uint16_t addr = smbus->command;
                log_trace("SMBUS WRITE BYTE_DATA: reg=0x%02X <- 0x%02X",
                    smbus->command, smbus->data->byte);
                m_data[addr % EEPROM_SIZE] = smbus->data->byte;
                log_info(">>> SMBUS WRITE BYTE_DATA: calling Save()...");
                Save();
                log_info(">>> EepromBoard::Ioctl EXIT: SMBUS WRITE BYTE_DATA returning 0");
                return 0;
            }

            case I2C_SMBUS_WORD_DATA:
            {
                // Write word (2 bytes) to specified register
                uint16_t addr = smbus->command;
                log_trace("SMBUS WRITE WORD_DATA: reg=0x%02X <- 0x%04X",
                    smbus->command, smbus->data->word);
                m_data[addr % EEPROM_SIZE] = smbus->data->word & 0xFF;
                m_data[(addr + 1) % EEPROM_SIZE] = (smbus->data->word >> 8) & 0xFF;
                log_info(">>> SMBUS WRITE WORD_DATA: calling Save()...");
                Save();
                log_info(">>> EepromBoard::Ioctl EXIT: SMBUS WRITE WORD_DATA returning 0");
                return 0;
            }

            case I2C_SMBUS_BLOCK_DATA:
            case I2C_SMBUS_I2C_BLOCK_BROKEN:
            case I2C_SMBUS_I2C_BLOCK_DATA:
            {
                log_info(">>> SMBUS WRITE BLOCK: START processing");

                // Lindbergh I2C Block Write Protocol:
                // - smbus->command = page/bank selector (0x01, 0x03, 0x10, etc.)
                // - block[0] = total length including address byte
                // - block[1] = base address within EEPROM
                // - block[2...] = actual data to write

                uint8_t totalLen = smbus->data->block[0];
                log_info(">>> SMBUS WRITE BLOCK: totalLen=%d", totalLen);

                if (totalLen > 32) totalLen = 32;
                if (totalLen < 1) {
                    log_warn("SMBUS WRITE BLOCK: Invalid length %d", totalLen);
                    log_info(">>> EepromBoard::Ioctl EXIT: SMBUS WRITE BLOCK invalid len returning 0");
                    return 0;
                }

                // First byte of block data is the EEPROM address
                uint8_t eepromAddr = smbus->data->block[1];
                uint8_t dataLen = totalLen - 1;  // Subtract address byte

                log_trace("SMBUS WRITE BLOCK: cmd=0x%02X addr=0x%02X len=%d <- [0x%02X 0x%02X 0x%02X ...]",
                    smbus->command, eepromAddr, dataLen,
                    dataLen > 0 ? smbus->data->block[2] : 0,
                    dataLen > 1 ? smbus->data->block[3] : 0,
                    dataLen > 2 ? smbus->data->block[4] : 0);

                log_info(">>> SMBUS WRITE BLOCK: Writing %d bytes to addr 0x%02X", dataLen, eepromAddr);

                // Write data to EEPROM (block[2] onwards is actual data)
                for (int i = 0; i < dataLen; i++) {
                    uint16_t writeAddr = ((uint16_t)eepromAddr + i) % EEPROM_SIZE;
                    m_data[writeAddr] = smbus->data->block[i + 2];
                }

                log_info(">>> SMBUS WRITE BLOCK: calling Save()...");
                Save();
                log_info(">>> EepromBoard::Ioctl EXIT: SMBUS WRITE BLOCK returning 0");
                return 0;
            }

            default:
                log_warn("SMBUS WRITE: Unknown size %d", smbus->size);
                log_info(">>> EepromBoard::Ioctl EXIT: SMBUS WRITE unknown size returning 0");
                return 0;
            }
        }

        // Should not reach here, but just in case
        log_info(">>> EepromBoard::Ioctl EXIT: I2C_SMBUS end of function returning 0");
        return 0;
    }

    default:
        log_warn("EepromBoard: Unknown ioctl request 0x%lX", request);
        log_info(">>> EepromBoard::Ioctl EXIT: Unknown request returning -1");
        return -1;
    }
}
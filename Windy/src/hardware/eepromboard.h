#pragma once

#include <stdio.h>
#include <stdint.h>
#include <cstring>
#include <string>

// I2C / SMBus Definitions
#define I2C_SLAVE 0x0703
#define I2C_SMBUS_BLOCK_MAX 32
#define I2C_GET_FUNCTIONS 0x705
#define I2C_SMBUS_TRANSFER 0x720
#define I2C_SET_SLAVE_MODE 0x703
#define I2C_BUFFER_CLEAR 0x1261
#define I2C_READ 1
#define I2C_SEEK 2
#define I2C_WRITE 3

// EEPROM Constants
enum EepromSection {
    SECTION_STATIC,
    SECTION_NETWORK_TYPE,
    SECTION_ETH0,
    SECTION_ETH1,
    SECTION_CREDIT,
    SECTION_BACKUP,
    SECTION_COUNT
};

struct EepromOffsets {
    uint16_t offset;
    uint16_t size;
};

// Data structures for I2C ioctl
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

class EepromBoard {
public:
    EepromBoard();
    ~EepromBoard();

    bool Open(const char* path = "eeprom.bin");
    void Close();

    int Ioctl(unsigned int request, void* data);

    // Configuration Setters (to replace getConfig() calls)
    void SetKeyChip(const char* keychip);
    void SetRegion(int region);
    void SetFreeplay(bool enabled);
    void SetNetworkIP(const char* ip, const char* mask);
    void EnableEmulationPatches(uint32_t gameId);

private:
    // Internal Logic from eepromSettings.c
    void BuildCrc32Table();
    uint32_t GenCrc(int section, size_t n);
    void AddCrcToBuffer(int section);
    int CheckCrcInBuffer(int section);

    int FillBuffer();
    int WriteSectionToFile(int section);

    // Section Creation
    int CreateStaticSection();
    int CreateNetworkTypeSection();
    int CreateEthSection(int section);
    int CreateCreditSection();

    // Specific Game Fixes
    int FixCreditSection();
    int FixCoinAssignmentsHummer();

    // Helper
    uint32_t IpToUInt(const char* ip);

    // Member Variables
    FILE* m_file;
    std::string m_path;
    unsigned char m_buffer[512]; // EEPROM content cache
    uint32_t m_crc32Table[256];

    // Config state
    char m_keychip[17];
    int m_region;
    bool m_freeplay;
    std::string m_ip;
    std::string m_netmask;
    uint32_t m_gameId; // For patching logic
};
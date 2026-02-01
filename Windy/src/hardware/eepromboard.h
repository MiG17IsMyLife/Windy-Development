#pragma once

#include <stdio.h>
#include <stdint.h>
#include <cstring>
#include <string>

// I2C / SMBus Definitions
#define I2C_SMBUS_BLOCK_MAX 32
#define I2C_GET_FUNCTIONS   0x705
#define I2C_SMBUS_TRANSFER  0x720
#define I2C_SET_SLAVE_MODE  0x703
#define I2C_BUFFER_CLEAR    0x1261
#define I2C_READ            1
#define I2C_SEEK            2
#define I2C_WRITE           3

// EEPROM Section Enum
enum EepromSection {
    SECTION_STATIC = 0,
    SECTION_NETWORK_TYPE,
    SECTION_ETH0,
    SECTION_ETH1,
    SECTION_CREDIT,
    SECTION_BACKUP,
    SECTION_COUNT
};

// EEPROM Offset Table Entry
struct EepromOffsets {
    uint16_t offset;
    uint16_t size;
};

// I2C data structures
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

/**
 * @brief EEPROM Board emulation
 * Handles I2C EEPROM emulation for Lindbergh games.
 */
class EepromBoard {
public:
    EepromBoard();
    ~EepromBoard();

    bool Open(const char* path = "eeprom.bin");
    void Close();
    int Ioctl(unsigned int request, void* data);

    // Configuration
    void SetRegion(int region) { m_configRegion = region; }
    void SetFreeplay(int freeplay) { m_configFreeplay = freeplay; }
    void SetGameId(uint32_t gameId) { m_gameId = gameId; }
    void SetNetworkIP(const char* ip, const char* mask);
    void SetOR2Network(const char* ip, const char* mask);
    void SetEnableNetworkPatches(bool enable) { m_enableNetworkPatches = enable; }

    // Public accessors
    int GetRegion();
    int GetFreeplay();

    // Game-specific fixes
    int FixCreditSection();
    int FixCoinAssignmentsHummer();
    int SetIP(const char* ipAddress, const char* netMask);
    void EnableEmulationPatches(uint32_t gameId);

private:
    // CRC functions
    void BuildCrc32Table();
    uint32_t GenCrc(int section, size_t n);
    void AddCrcToBuffer(int section);
    int CheckCrcInBuffer(int section);

    // File operations
    int FillBuffer();
    int WriteSectionToFile(int section);

    // Section creation
    int CreateStaticSection();
    int CreateNetworkTypeSection();
    int CreateEthSection(int section);
    int CreateCreditSection();

    // Internal setters
    int SetRegionInternal(int region);
    int SetFreeplayInternal(int freeplay);

    // Settings init
    int SettingsInit();

    // IP conversion helper (matches inet_addr behavior)
    uint32_t IpToUInt(const char* ip);

    // Member variables
    FILE* m_file;
    std::string m_path;

    unsigned char m_buffer[512];
    uint32_t m_crc32Table[256];

    int m_configRegion;
    int m_configFreeplay;
    uint32_t m_gameId;
    std::string m_networkIP;
    std::string m_networkMask;
    std::string m_or2IP;
    std::string m_or2Mask;
    bool m_enableNetworkPatches;
};

extern const EepromOffsets g_eepromOffsetTable[];
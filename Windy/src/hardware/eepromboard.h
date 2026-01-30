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

    /**
     * @brief Initialize EEPROM (matches initEeprom)
     * @param path Path to eeprom.bin file
     * @return true if successful
     */
    bool Open(const char* path = "eeprom.bin");

    /**
     * @brief Close EEPROM file
     */
    void Close();

    /**
     * @brief Handle I2C IOCTL requests (matches eepromIoctl() from Lindbergh-loader)
     * @param request IOCTL request code
     * @param data Request data
     * @return 0 on success
     */
    int Ioctl(unsigned int request, void* data);

    // ============================================================
    // Configuration
    // ============================================================

    void SetRegion(int region) { m_configRegion = region; }
    void SetFreeplay(int freeplay) { m_configFreeplay = freeplay; }
    void SetGameId(uint32_t gameId) { m_gameId = gameId; }
    void SetNetworkIP(const char* ip, const char* mask);
    void SetOR2Network(const char* ip, const char* mask);
    void SetEnableNetworkPatches(bool enable) { m_enableNetworkPatches = enable; }

    // ============================================================
    // eepromSettings.h functions (public for external access)
    // ============================================================

    int GetRegion();
    int GetFreeplay();

    // ============================================================
    // Game-specific fixes (public for main.cpp to call)
    // ============================================================

    /**
     * @brief Fix credit section for LGJ/HOD4 Special
     * Sets service type to 0 and freeplay mode
     */
    int FixCreditSection();

    /**
     * @brief Fix coin assignments for Hummer series
     * Resets coin chute type and service type
     */
    int FixCoinAssignmentsHummer();

    /**
     * @brief Set IP address in EEPROM (for OutRun 2 SP SDX network)
     */
    int SetIP(const char* ipAddress, const char* netMask);

    /**
     * @brief Enable game-specific emulation patches
     * Called from main.cpp after game detection
     */
    void EnableEmulationPatches(uint32_t gameId);

private:
    // ============================================================
    // eepromSettings.c functions (internal)
    // ============================================================

    void BuildCrc32Table();
    uint32_t GenCrc(int section, size_t n);
    void AddCrcToBuffer(int section);
    int CheckCrcInBuffer(int section);

    int FillBuffer();
    int WriteSectionToFile(int section);

    // Section creation
    int CreateStaticSection();
    int CreateNetworkTypeSection();
    int CreateEthSection(int section);
    int CreateCreditSection();

    // Region/Freeplay setters (internal, write to file)
    int SetRegionInternal(int region);
    int SetFreeplayInternal(int freeplay);

    // eepromSettingsInit equivalent
    int SettingsInit();

    // Helper
    uint32_t IpToUInt(const char* ip);

    // ============================================================
    // Member Variables
    // ============================================================

    FILE* m_file;
    std::string m_path;

    // EEPROM buffer (matches eepromBuffer[512])
    unsigned char m_buffer[512];

    // CRC32 table (matches crc32Table[255])
    uint32_t m_crc32Table[256];

    // Configuration (replaces getConfig() calls)
    int m_configRegion;      // -1 = don't change
    int m_configFreeplay;    // -1 = don't change
    uint32_t m_gameId;
    std::string m_networkIP;
    std::string m_networkMask;
    std::string m_or2IP;
    std::string m_or2Mask;
    bool m_enableNetworkPatches;
};

// ============================================================
// Global EEPROM offset table
// ============================================================
extern const EepromOffsets g_eepromOffsetTable[];
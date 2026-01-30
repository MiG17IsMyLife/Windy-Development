#define _CRT_SECURE_NO_WARNINGS

#include "eepromboard.h"
#include "../core/log.h"
#include "../core/config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================
// Global EEPROM offset table
// ============================================================
const EepromOffsets g_eepromOffsetTable[] = {
    {0x0000, 0x0020},  // amSysDataRecord_Static
    {0x00A0, 0x0010},  // amSysDataRecord_NetworkType
    {0x00B0, 0x0020},  // amSysDataRecord_Eth0
    {0x00D0, 0x0020},  // amSysDataRecord_Eth1
    {0x0100, 0x0040},  // amSysDataRecord_Credit
    {0x0400, 0x0060}   // amSysDataRecord_Backup
};

// ============================================================
// Constructor / Destructor
// ============================================================

EepromBoard::EepromBoard()
    : m_file(nullptr)
    , m_configRegion(-1)
    , m_configFreeplay(-1)
    , m_gameId(0)
    , m_enableNetworkPatches(false)
{
    memset(m_buffer, 0, sizeof(m_buffer));
    memset(m_crc32Table, 0, sizeof(m_crc32Table));
}

EepromBoard::~EepromBoard() {
    Close();
}

// ============================================================
// Open / Close (matches initEeprom)
// ============================================================

bool EepromBoard::Open(const char* path) {
    m_path = path;

    // Create file if it doesn't exist
    m_file = fopen(path, "a");
    if (m_file == nullptr) {
        log_error("EepromBoard: Cannot open %s", path);
        return false;
    }
    fclose(m_file);

    m_file = fopen(path, "rb+");
    if (m_file == nullptr) {
        log_error("EepromBoard: Cannot open %s for read/write", path);
        return false;
    }

    // Initialize eeprom settings)
    if (SettingsInit() != 0) {
        log_error("EepromBoard: Error initializing eeprom settings.");
        fclose(m_file);
        m_file = nullptr;
        return false;
    }

    // Apply region from config
    if (m_configRegion != -1) {
        if (GetRegion() != m_configRegion) {
            SetRegionInternal(m_configRegion);
        }
    }

    // Apply freeplay from config
    if (m_configFreeplay != -1) {
        if (GetFreeplay() != m_configFreeplay) {
            SetFreeplayInternal(m_configFreeplay);
        }
    }

    // Game-specific fixes
    switch (m_gameId) {
    case LETS_GO_JUNGLE_SPECIAL:
    case THE_HOUSE_OF_THE_DEAD_EX:
    case THE_HOUSE_OF_THE_DEAD_4_SPECIAL:
    case THE_HOUSE_OF_THE_DEAD_4_SPECIAL_REVB:
        if (FixCreditSection() != 0) {
            log_error("EepromBoard: Error fixing credit section.");
            fclose(m_file);
            m_file = nullptr;
            return false;
        }
        break;

    case HUMMER:
    case HUMMER_SDLX:
    case HUMMER_EXTREME:
    case HUMMER_EXTREME_MDX:
        if (FixCoinAssignmentsHummer() != 0) {
            log_error("EepromBoard: Error fixing coin assignments for Hummer.");
            fclose(m_file);
            m_file = nullptr;
            return false;
        }
        break;

    case OUTRUN_2_SP_SDX:
    case OUTRUN_2_SP_SDX_REVA:
        if (m_enableNetworkPatches && !m_or2IP.empty() && !m_or2Mask.empty()) {
            if (SetIP(m_or2IP.c_str(), m_or2Mask.c_str()) != 0) {
                log_error("EepromBoard: Error setting OR2 IP address.");
                fclose(m_file);
                m_file = nullptr;
                return false;
            }
        }
        break;

    default:
        break;
    }

    fseek(m_file, 0, SEEK_SET);

    log_info("EepromBoard: Initialized successfully (%s)", path);
    return true;
}

void EepromBoard::Close() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}

// ============================================================
// Ioctl (matches eepromIoctl)
// ============================================================

int EepromBoard::Ioctl(unsigned int request, void* data) {
    switch (request) {
    case I2C_GET_FUNCTIONS: {
        // Copied from what SEGABOOT expects (matches Lindbergh-loader)
        uint32_t* functions = (uint32_t*)data;
        functions[0] = 0x20000 | 0x40000 | 0x100000 | 0x400000 | 0x8000000;
        // Additional flags from House of the Dead 4 init sequence
        functions[0] = functions[0] | 0x20000 | 0x40000 | 0x80000 |
            0x100000 | 0x200000 | 0x400000 | 0x1000000 | 0x2000000;
        break;
    }

    case I2C_SMBUS_TRANSFER: {
        struct i2c_smbus_ioctl_data* _data = (struct i2c_smbus_ioctl_data*)data;

        switch (_data->size) {
        case I2C_READ: {
            if (m_file) {
                fread(&_data->data->byte, 1, sizeof(char), m_file);
            }
            break;
        }

        case I2C_SEEK: {
            uint16_t address = (_data->command & 0xFF) << 8 | (_data->data->byte & 0xFF);
            if (m_file) {
                fseek(m_file, address, SEEK_SET);
            }
            break;
        }

        case I2C_WRITE: {
            uint16_t address = (_data->command & 0xFF) << 8 | (_data->data->byte & 0xFF);
            char val = _data->data->word >> 8 & 0xFF;
            if (m_file) {
                fseek(m_file, address, SEEK_SET);
                fwrite(&val, 1, sizeof(char), m_file);
            }
            break;
        }

        default:
            log_error("EepromBoard: Incorrect I2C transfer size %d", _data->size);
            break;
        }
        break;
    }

    case I2C_SET_SLAVE_MODE:
    case I2C_BUFFER_CLEAR:
        // No action needed
        break;

    default:
        log_warn("EepromBoard: Unknown I2C ioctl 0x%X", request);
        break;
    }

    return 0;
}

// ============================================================
// Configuration setters
// ============================================================

void EepromBoard::SetNetworkIP(const char* ip, const char* mask) {
    if (ip) m_networkIP = ip;
    if (mask) m_networkMask = mask;
}

void EepromBoard::SetOR2Network(const char* ip, const char* mask) {
    if (ip) m_or2IP = ip;
    if (mask) m_or2Mask = mask;
}

// ============================================================
// eepromSettings.c functions
// ============================================================

void EepromBoard::BuildCrc32Table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t ch = i;
        uint32_t crc = 0;
        for (size_t j = 0; j < 8; j++) {
            uint32_t b = (ch ^ crc) & 1;
            crc >>= 1;
            if (b) crc = crc ^ 0xEDB88320;
            ch >>= 1;
        }
        m_crc32Table[i] = crc;
    }
}

uint32_t EepromBoard::GenCrc(int section, size_t n) {
    unsigned char* buff = &m_buffer[g_eepromOffsetTable[section].offset + 4];
    unsigned long crc = 0xfffffffful;
    for (size_t i = 0; i < n; i++) {
        crc = m_crc32Table[*buff++ ^ (crc & 0xff)] ^ (crc >> 8);
    }
    return (uint32_t)crc;
}

void EepromBoard::AddCrcToBuffer(int section) {
    uint32_t crc = GenCrc(section, g_eepromOffsetTable[section].size - 4);
    memcpy(&m_buffer[g_eepromOffsetTable[section].offset], &crc, sizeof(uint32_t));
}

int EepromBoard::CheckCrcInBuffer(int section) {
    uint32_t crc;
    memcpy(&crc, &m_buffer[g_eepromOffsetTable[section].offset], 4);
    if (crc != GenCrc(section, g_eepromOffsetTable[section].size - 4))
        return 1;
    return 0;
}

int EepromBoard::FillBuffer() {
    if (!m_file) return 1;
    fseek(m_file, 0, SEEK_SET);
    int b = (int)fread(m_buffer, 1, 512, m_file);
    if (b < 512) return 1;
    return 0;
}

int EepromBoard::WriteSectionToFile(int section) {
    if (!m_file) return 1;

    unsigned char* buff = &m_buffer[g_eepromOffsetTable[section].offset];

    // Original
    if (fseek(m_file, g_eepromOffsetTable[section].offset, SEEK_SET) != 0)
        return 1;
    if (fwrite(buff, g_eepromOffsetTable[section].size, 1, m_file) != 1)
        return 1;

    // Duplicate (at +0x200)
    if (fseek(m_file, g_eepromOffsetTable[section].offset + 0x200, SEEK_SET) != 0)
        return 1;
    if (fwrite(buff, g_eepromOffsetTable[section].size, 1, m_file) != 1)
        return 1;

    fflush(m_file);
    return 0;
}

int EepromBoard::CreateStaticSection() {
    unsigned char* buff = &m_buffer[g_eepromOffsetTable[SECTION_STATIC].offset];
    memset(buff, 0, g_eepromOffsetTable[SECTION_STATIC].size);
    buff[14] = 0;  // Region (Japan default)
    // Keychip ID (matches Lindbergh-loader: "AALG-TG-933F3904")
    memcpy(buff + 15, "AALG-TG-933F3904", 16);
    return 0;
}

int EepromBoard::CreateNetworkTypeSection() {
    memset(&m_buffer[g_eepromOffsetTable[SECTION_NETWORK_TYPE].offset], 0,
        g_eepromOffsetTable[SECTION_NETWORK_TYPE].size);
    return 0;
}

int EepromBoard::CreateEthSection(int section) {
    unsigned char* buff = &m_buffer[g_eepromOffsetTable[section].offset];
    memset(buff, 0, g_eepromOffsetTable[section].size);

    uint32_t value = 1;
    memcpy(buff + 8, &value, sizeof(uint32_t));

    value = IpToUInt("10.0.0.1");
    if (section == SECTION_ETH1)
        value += (1 << 24);  // 11.0.0.1
    memcpy(buff + 12, &value, sizeof(uint32_t));

    value = IpToUInt("255.255.255.0");
    memcpy(buff + 16, &value, sizeof(uint32_t));

    value = IpToUInt("0.0.0.0");
    memcpy(buff + 20, &value, sizeof(uint32_t));
    memcpy(buff + 24, &value, sizeof(uint32_t));
    memcpy(buff + 28, &value, sizeof(uint32_t));

    return 0;
}

int EepromBoard::CreateCreditSection() {
    unsigned char* buff = &m_buffer[g_eepromOffsetTable[SECTION_CREDIT].offset];
    memset(buff, 0, g_eepromOffsetTable[SECTION_CREDIT].size);

    buff[32] = 99;
    buff[33] = 9;
    buff[34] = 4;
    buff[35] = 0;  // Coin chute type [COMMON (Default) / INDIVIDUAL]
    buff[36] = 1;  // Service Type [COMMON / INDIVIDUAL (Default)]
    buff[38] = 1;
    buff[39] = 0;  // Freeplay set to 1 (Note: 0 = freeplay enabled in some contexts)
    buff[40] = 1;  // Credits Chute #1
    buff[41] = 1;  // Credits Chute #1
    buff[42] = 0;
    buff[43] = 1;  // Coins

    for (size_t i = 0; i < 8; i++) {
        buff[44 + i] = 1;
    }

    return 0;
}

int EepromBoard::GetRegion() {
    return m_buffer[14];
}

int EepromBoard::GetFreeplay() {
    return m_buffer[g_eepromOffsetTable[SECTION_CREDIT].offset + 39];
}

int EepromBoard::SetRegionInternal(int region) {
    m_buffer[14] = (unsigned char)region;
    AddCrcToBuffer(SECTION_STATIC);
    if (WriteSectionToFile(SECTION_STATIC) != 0) {
        log_error("EepromBoard: Error writing region to eeprom.");
        return 1;
    }
    return 0;
}

int EepromBoard::SetFreeplayInternal(int freeplay) {
    if (CreateCreditSection() != 0) {
        log_error("EepromBoard: Error setting freeplay.");
        return 1;
    }
    m_buffer[g_eepromOffsetTable[SECTION_CREDIT].offset + 39] = (unsigned char)freeplay;
    AddCrcToBuffer(SECTION_CREDIT);
    if (WriteSectionToFile(SECTION_CREDIT) != 0) {
        log_error("EepromBoard: Error writing freeplay to eeprom.");
        return 1;
    }
    return 0;
}

int EepromBoard::FixCreditSection() {
    m_buffer[g_eepromOffsetTable[SECTION_CREDIT].offset + 36] = 0;
    m_buffer[g_eepromOffsetTable[SECTION_CREDIT].offset + 39] = 0;
    AddCrcToBuffer(SECTION_CREDIT);
    if (WriteSectionToFile(SECTION_CREDIT) != 0) {
        log_error("EepromBoard: Error writing credit section fix.");
        return 1;
    }
    return 0;
}

int EepromBoard::FixCoinAssignmentsHummer() {
    m_buffer[g_eepromOffsetTable[SECTION_CREDIT].offset + 35] = 0;
    m_buffer[g_eepromOffsetTable[SECTION_CREDIT].offset + 36] = 0;
    AddCrcToBuffer(SECTION_CREDIT);
    if (WriteSectionToFile(SECTION_CREDIT) != 0) {
        log_error("EepromBoard: Error writing Hummer coin fix.");
        return 1;
    }
    return 0;
}

int EepromBoard::SetIP(const char* ipAddress, const char* netMask) {
    uint32_t ipHex = IpToUInt(ipAddress);
    uint32_t nMask = IpToUInt(netMask);
    uint32_t dhcp = 0;

    memcpy(&m_buffer[g_eepromOffsetTable[SECTION_ETH0].offset + 8], &dhcp, sizeof(uint32_t));
    memcpy(&m_buffer[g_eepromOffsetTable[SECTION_ETH0].offset + 12], &ipHex, sizeof(uint32_t));
    memcpy(&m_buffer[g_eepromOffsetTable[SECTION_ETH0].offset + 16], &nMask, sizeof(uint32_t));

    AddCrcToBuffer(SECTION_ETH0);
    if (WriteSectionToFile(SECTION_ETH0) != 0) {
        log_error("EepromBoard: Error writing IP address.");
        return 1;
    }
    return 0;
}

int EepromBoard::SettingsInit() {
    // Matches eepromSettingsInit
    BuildCrc32Table();

    fseek(m_file, 0, SEEK_END);
    int size = (int)ftell(m_file);
    fseek(m_file, 0, SEEK_SET);

    if (size >= 832) {
        // EEPROM initialized at least by SEGABOOT
        FillBuffer();

        if (CheckCrcInBuffer(SECTION_STATIC) != 0) {
            if (CreateStaticSection() != 0) {
                log_error("EepromBoard: Error creating Static Section.");
                return 1;
            }
            AddCrcToBuffer(SECTION_STATIC);
            if (WriteSectionToFile(SECTION_STATIC) != 0) {
                log_error("EepromBoard: Error writing STATIC section.");
                return 1;
            }
        }

        if (CheckCrcInBuffer(SECTION_NETWORK_TYPE) != 0) {
            if (CreateNetworkTypeSection() != 0) {
                log_error("EepromBoard: Error creating NetworkType Section.");
                return 1;
            }
            AddCrcToBuffer(SECTION_NETWORK_TYPE);
            if (WriteSectionToFile(SECTION_NETWORK_TYPE) != 0) {
                log_error("EepromBoard: Error writing NETWORK_TYPE section.");
                return 1;
            }
        }

        if (CheckCrcInBuffer(SECTION_ETH0) != 0) {
            if (CreateEthSection(SECTION_ETH0) != 0) {
                log_error("EepromBoard: Error creating ETH0 Section.");
                return 1;
            }
            AddCrcToBuffer(SECTION_ETH0);
            if (WriteSectionToFile(SECTION_ETH0) != 0) {
                log_error("EepromBoard: Error writing ETH0 section.");
                return 1;
            }
        }

        if (CheckCrcInBuffer(SECTION_ETH1) != 0) {
            if (CreateEthSection(SECTION_ETH1) != 0) {
                log_error("EepromBoard: Error creating ETH1 Section.");
                return 1;
            }
            AddCrcToBuffer(SECTION_ETH1);
            if (WriteSectionToFile(SECTION_ETH1) != 0) {
                log_error("EepromBoard: Error writing ETH1 section.");
                return 1;
            }
        }

        if (CheckCrcInBuffer(SECTION_CREDIT) != 0) {
            if (CreateCreditSection() != 0) {
                log_error("EepromBoard: Error creating CREDIT Section.");
                return 1;
            }
            AddCrcToBuffer(SECTION_CREDIT);
            if (WriteSectionToFile(SECTION_CREDIT) != 0) {
                log_error("EepromBoard: Error writing CREDIT section.");
                return 1;
            }
        }
    }
    else {
        // Create all sections from scratch
        log_info("EepromBoard: Creating new eeprom layout.");

        if (CreateStaticSection() != 0 ||
            CreateNetworkTypeSection() != 0 ||
            CreateEthSection(SECTION_ETH0) != 0 ||
            CreateEthSection(SECTION_ETH1) != 0 ||
            CreateCreditSection() != 0) {
            log_error("EepromBoard: Error creating sections from scratch.");
            return 1;
        }

        AddCrcToBuffer(SECTION_STATIC);
        AddCrcToBuffer(SECTION_NETWORK_TYPE);
        AddCrcToBuffer(SECTION_ETH0);
        AddCrcToBuffer(SECTION_ETH1);
        AddCrcToBuffer(SECTION_CREDIT);

        if (WriteSectionToFile(SECTION_STATIC) != 0 ||
            WriteSectionToFile(SECTION_NETWORK_TYPE) != 0 ||
            WriteSectionToFile(SECTION_ETH0) != 0 ||
            WriteSectionToFile(SECTION_ETH1) != 0 ||
            WriteSectionToFile(SECTION_CREDIT) != 0) {
            log_error("EepromBoard: Error writing sections from scratch.");
            return 1;
        }
    }

    return 0;
}

uint32_t EepromBoard::IpToUInt(const char* ip) {
    unsigned int a, b, c, d;
    if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        // Little endian construction (matches inet_addr behavior)
        return (d << 24) | (c << 16) | (b << 8) | a;
    }
    return 0;
}

// ============================================================
// Emulation Patches
// ============================================================

void EepromBoard::EnableEmulationPatches(uint32_t gameId) {
    m_gameId = gameId;
}
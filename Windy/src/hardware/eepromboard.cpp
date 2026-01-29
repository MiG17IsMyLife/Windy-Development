#define _CRT_SECURE_NO_WARNINGS

#include "EepromBoard.h"
#include "../core/log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const EepromOffsets eepromOffsetTable[] = {
    {0x0000, 0x0020},  // amSysDataRecord_Static
    {0x00A0, 0x0010},  // amSysDataRecord_NetworkType
    {0x00B0, 0x0020},  // amSysDataRecord_Eth0
    {0x00D0, 0x0020},  // amSysDataRecord_Eth1
    {0x0100, 0x0040},  // amSysDataRecord_Credit
    {0x0400, 0x0060}   // amSysDataRecord_Backup
};

EepromBoard::EepromBoard()
    : m_file(nullptr)
    , m_region(1) // US default
    , m_freeplay(false)
    , m_gameId(0)
{
    memset(m_buffer, 0, sizeof(m_buffer));
    memset(m_keychip, 0, sizeof(m_keychip));
    strcpy(m_keychip, "AAGX-01A00009999"); // Default Keychip

    // Default Network
    m_ip = "192.168.1.2";
    m_netmask = "255.255.255.0";

    BuildCrc32Table();
}

EepromBoard::~EepromBoard() {
    Close();
}

bool EepromBoard::Open(const char* path) {
    m_path = path;
    log_info("EepromBoard: Opening %s", path);

    // Try to open existing, or create new
    m_file = fopen(m_path.c_str(), "rb+");
    if (!m_file) {
        log_warn("EepromBoard: File not found, creating new eeprom.");
        m_file = fopen(m_path.c_str(), "wb+");
    }

    if (!m_file) {
        log_error("EepromBoard: Failed to open eeprom file.");
        return false;
    }

    fseek(m_file, 0, SEEK_END);
    long size = ftell(m_file);
    fseek(m_file, 0, SEEK_SET);

    bool initialized = (size >= 512);

    if (initialized) {
        FillBuffer();

        // Verify and Repair Sections
        if (CheckCrcInBuffer(SECTION_STATIC) != 0) {
            log_warn("EepromBoard: Invalid Static Section, recreating.");
            CreateStaticSection();
            AddCrcToBuffer(SECTION_STATIC);
            WriteSectionToFile(SECTION_STATIC);
        }
        if (CheckCrcInBuffer(SECTION_NETWORK_TYPE) != 0) {
            CreateNetworkTypeSection();
            AddCrcToBuffer(SECTION_NETWORK_TYPE);
            WriteSectionToFile(SECTION_NETWORK_TYPE);
        }
        if (CheckCrcInBuffer(SECTION_ETH0) != 0) {
            CreateEthSection(SECTION_ETH0);
            AddCrcToBuffer(SECTION_ETH0);
            WriteSectionToFile(SECTION_ETH0);
        }
        if (CheckCrcInBuffer(SECTION_ETH1) != 0) {
            CreateEthSection(SECTION_ETH1);
            AddCrcToBuffer(SECTION_ETH1);
            WriteSectionToFile(SECTION_ETH1);
        }
        if (CheckCrcInBuffer(SECTION_CREDIT) != 0) {
            CreateCreditSection();
            AddCrcToBuffer(SECTION_CREDIT);
            WriteSectionToFile(SECTION_CREDIT);
        }
    }
    else {
        // Create All Scratch
        log_info("EepromBoard: Creating full eeprom layout.");
        CreateStaticSection();
        CreateNetworkTypeSection();
        CreateEthSection(SECTION_ETH0);
        CreateEthSection(SECTION_ETH1);
        CreateCreditSection();

        AddCrcToBuffer(SECTION_STATIC);
        AddCrcToBuffer(SECTION_NETWORK_TYPE);
        AddCrcToBuffer(SECTION_ETH0);
        AddCrcToBuffer(SECTION_ETH1);
        AddCrcToBuffer(SECTION_CREDIT);

        WriteSectionToFile(SECTION_STATIC);
        WriteSectionToFile(SECTION_NETWORK_TYPE);
        WriteSectionToFile(SECTION_ETH0);
        WriteSectionToFile(SECTION_ETH1);
        WriteSectionToFile(SECTION_CREDIT);
    }

    // Apply Patches (Config Override)
    // Region
    if (m_buffer[14] != m_region) {
        m_buffer[14] = (uint8_t)m_region;
        AddCrcToBuffer(SECTION_STATIC);
        WriteSectionToFile(SECTION_STATIC);
    }

    // Network Patching (Force config IP)
    // Note: Lindbergh-loader logic for setIP
    {
        uint32_t ipHex = IpToUInt(m_ip.c_str());
        uint32_t nMask = IpToUInt(m_netmask.c_str());
        uint32_t dhcp = 0;

        // Check if different to avoid rewriting if not needed
        uint32_t currentIp;
        memcpy(&currentIp, &m_buffer[eepromOffsetTable[SECTION_ETH0].offset + 12], 4);

        if (currentIp != ipHex) {
            memcpy(&m_buffer[eepromOffsetTable[SECTION_ETH0].offset + 8], &dhcp, 4);
            memcpy(&m_buffer[eepromOffsetTable[SECTION_ETH0].offset + 12], &ipHex, 4);
            memcpy(&m_buffer[eepromOffsetTable[SECTION_ETH0].offset + 16], &nMask, 4);
            AddCrcToBuffer(SECTION_ETH0);
            WriteSectionToFile(SECTION_ETH0);
        }
    }

    return true;
}

void EepromBoard::Close() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}

int EepromBoard::Ioctl(unsigned int request, void* data) {
    if (!m_file) return -1;

    switch (request) {
    case I2C_SLAVE:
        // In a real driver, this sets the target address.
        // We assume 0x50 or 0x51 is being addressed.
        return 0;

    case I2C_SMBUS_TRANSFER:
        if (data) {
            i2c_smbus_ioctl_data* args = (i2c_smbus_ioctl_data*)data;
            uint8_t command = args->command; // EEPROM Memory Address

            if (args->read_write == I2C_READ) {
                // Read byte
                if (args->data) {
                    args->data->byte = m_buffer[command];
                }
            }
            else {
                // Write byte
                if (args->data) {
                    m_buffer[command] = args->data->byte;
                    // Sync to file immediately (slow but safe)
                    fseek(m_file, command, SEEK_SET);
                    fwrite(&m_buffer[command], 1, 1, m_file);

                    // Shadow copy at +0x200 (Mirrored EEPROM)
                    // Lindbergh-loader does this in WriteSectionToFile, 
                    // but for individual byte writes via I2C, real HW might not duplicate automatically?
                    // Loader's eeprom.c uses pwrite without duplication.
                    // However, settings code duplicates. We'll stick to single write for runtime I2C.
                    fflush(m_file);
                }
            }
        }
        return 0;

    default:
        // log_warn("EepromBoard: Unknown IOCTL 0x%X", request);
        break;
    }
    return -1;
}

// Configuration Setters
void EepromBoard::SetKeyChip(const char* keychip) {
    strncpy(m_keychip, keychip, sizeof(m_keychip) - 1);
}

void EepromBoard::SetRegion(int region) {
    m_region = region;
}

void EepromBoard::SetFreeplay(bool enabled) {
    m_freeplay = enabled;
}

void EepromBoard::SetNetworkIP(const char* ip, const char* mask) {
    m_ip = ip;
    m_netmask = mask;
}

void EepromBoard::EnableEmulationPatches(uint32_t gameId) {
    m_gameId = gameId;
    // Apply game specific fixes if needed immediately
}

// Internal Logic (Ported from eepromSettings.c)

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
    unsigned char* buff = &m_buffer[eepromOffsetTable[section].offset + 4];
    unsigned long crc = 0xfffffffful;
    for (size_t i = 0; i < n; i++) {
        crc = m_crc32Table[*buff++ ^ (crc & 0xff)] ^ (crc >> 8);
    }
    return crc;
}

void EepromBoard::AddCrcToBuffer(int section) {
    uint32_t crc = GenCrc(section, eepromOffsetTable[section].size - 4);
    memcpy(&m_buffer[eepromOffsetTable[section].offset], &crc, sizeof(uint32_t));
}

int EepromBoard::CheckCrcInBuffer(int section) {
    uint32_t crc;
    memcpy(&crc, &m_buffer[eepromOffsetTable[section].offset], 4);
    if (crc != GenCrc(section, eepromOffsetTable[section].size - 4))
        return 1;
    return 0;
}

int EepromBoard::FillBuffer() {
    if (!m_file) return 1;
    fseek(m_file, 0, SEEK_SET);
    size_t b = fread(m_buffer, 1, 512, m_file);
    if (b < 512) return 1;
    return 0;
}

int EepromBoard::WriteSectionToFile(int section) {
    if (!m_file) return 1;
    unsigned char* buff = &m_buffer[eepromOffsetTable[section].offset];

    fseek(m_file, eepromOffsetTable[section].offset, SEEK_SET);
    fwrite(buff, eepromOffsetTable[section].size, 1, m_file);

    fseek(m_file, eepromOffsetTable[section].offset + 0x200, SEEK_SET);
    fwrite(buff, eepromOffsetTable[section].size, 1, m_file);

    fflush(m_file);
    return 0;
}

int EepromBoard::CreateStaticSection() {
    unsigned char* buff = &m_buffer[eepromOffsetTable[SECTION_STATIC].offset];
    memset(buff, 0, eepromOffsetTable[SECTION_STATIC].size);
    buff[14] = (uint8_t)m_region;
    // memcpy(buff + 15, "AAGX-01A00009999", 16);
    memcpy(buff + 15, m_keychip, 16);
    return 0;
}

int EepromBoard::CreateNetworkTypeSection() {
    memset(&m_buffer[eepromOffsetTable[SECTION_NETWORK_TYPE].offset], 0,
        eepromOffsetTable[SECTION_NETWORK_TYPE].size);
    return 0;
}

int EepromBoard::CreateEthSection(int section) {
    unsigned char* buff = &m_buffer[eepromOffsetTable[section].offset];
    memset(buff, 0, eepromOffsetTable[section].size);
    uint32_t value = 1;
    memcpy(buff + 8, &value, sizeof(uint32_t)); // Enable?

    // Defaults from loader
    value = IpToUInt("10.0.0.1");
    if (section == SECTION_ETH1) value += (1 << 24); // 11.0.0.1 ? Loader logic

    memcpy(buff + 12, &value, sizeof(uint32_t)); // IP
    value = IpToUInt("255.255.255.0");
    memcpy(buff + 16, &value, sizeof(uint32_t)); // Mask
    value = IpToUInt("0.0.0.0");
    memcpy(buff + 20, &value, sizeof(uint32_t)); // Gateway
    memcpy(buff + 24, &value, sizeof(uint32_t));
    memcpy(buff + 28, &value, sizeof(uint32_t));
    return 0;
}

int EepromBoard::CreateCreditSection() {
    unsigned char* buff = &m_buffer[eepromOffsetTable[SECTION_CREDIT].offset];
    memset(buff, 0, eepromOffsetTable[SECTION_CREDIT].size);
    buff[32] = 99;
    buff[33] = 9;
    buff[34] = 4;
    buff[35] = 0; // Coin chute type
    buff[36] = 1; // Service Type
    buff[38] = 1;
    buff[39] = m_freeplay ? 0 : 1; // Note: Lindbergh logic: 0=Freeplay in some contexts? 
    // Loader: buff[39] = 0; // Freeplay set to 1. Wait, loader comment says "Freeplay set to 1" but assigns 0?
    // Let's stick to loader default:
    // Loader src: buff[39] = 0; // Freeplay set to 1
    // Actually in setFreeplay it says: eepromBuffer... = freeplay;
    // We will trust the passed-in member variable.

    buff[40] = 1; // Credits Chute #1
    buff[41] = 1; // Credits Chute #1
    buff[42] = 0;
    buff[43] = 1; // Coins
    for (size_t i = 0; i < 8; i++) {
        buff[44 + i] = 1;
    }
    return 0;
}

int EepromBoard::FixCreditSection() {
    m_buffer[eepromOffsetTable[SECTION_CREDIT].offset + 36] = 0;
    m_buffer[eepromOffsetTable[SECTION_CREDIT].offset + 39] = 0;
    AddCrcToBuffer(SECTION_CREDIT);
    return WriteSectionToFile(SECTION_CREDIT);
}

int EepromBoard::FixCoinAssignmentsHummer() {
    m_buffer[eepromOffsetTable[SECTION_CREDIT].offset + 35] = 0;
    m_buffer[eepromOffsetTable[SECTION_CREDIT].offset + 36] = 0;
    AddCrcToBuffer(SECTION_CREDIT);
    return WriteSectionToFile(SECTION_CREDIT);
}

uint32_t EepromBoard::IpToUInt(const char* ip) {
    unsigned int a, b, c, d;
    if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        return (d << 24) | (c << 16) | (b << 8) | a; // Little Endian construction for memory copy
    }
    return 0;
}
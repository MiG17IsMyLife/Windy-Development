#define _CRT_SECURE_NO_WARNINGS

#include "config.h"
#include "gamedata.h"
#include "iniparser.h"
#include "log.h"

#include <cstring>
#include <cstdlib>
#include <cctype>
#include <string>
#include <cstdio>
#include <filesystem>

// SDL3 Key codes
#define KEY_1 30
#define KEY_2 31
#define KEY_5 34
#define KEY_6 35
#define KEY_A 4
#define KEY_B 5
#define KEY_C 6
#define KEY_D 7
#define KEY_F 9
#define KEY_G 10
#define KEY_H 11
#define KEY_I 12
#define KEY_J 13
#define KEY_K 14
#define KEY_L 15
#define KEY_N 17
#define KEY_S 22
#define KEY_V 25
#define KEY_X 27
#define KEY_Z 29
#define KEY_UP 82
#define KEY_DOWN 81
#define KEY_LEFT 80
#define KEY_RIGHT 79
#define KEY_F1 58
#define KEY_F2 59

// ============================================================
// ConfigManager Implementation
// ============================================================

ConfigManager &ConfigManager::Instance()
{
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager()
{
    InitDefaults();
}

ConfigManager::~ConfigManager()
{
}

void ConfigManager::InitDefaults()
{
    memset(&m_config, 0, sizeof(WindyConfig));

    // General
    strcpy(m_config.gamePath, "");
    strcpy(m_config.elfPath, "");
    strcpy(m_config.eepromPath, "eeprom.bin");
    strcpy(m_config.sramPath, "sram.bin");

    // Display
    m_config.width = 1360;
    m_config.height = 768;
    m_config.fullscreen = false;
    m_config.keepAspectRatio = true;
    m_config.stretchToFit = false;
    m_config.windowX = -1;
    m_config.windowY = -1;

    // Emulation
    m_config.emulateJVS = true;
    m_config.emulateRideboard = false;
    m_config.emulateDriveboard = false;
    m_config.emulateMotionboard = false;
    m_config.emulateCardReader = false;
    m_config.emulateTouchscreen = false;
    m_config.jvsIOType = SEGA_TYPE_3;
    strcpy(m_config.jvsPath, "");

    // Region
    m_config.region = REGION_US;
    m_config.freeplay = true;

    // Graphics
    m_config.enableGPUSpoofing = true;
    m_config.fpsLimit = 0.0;
    m_config.vsync = false;

    // Debug
    m_config.showDebugMessages = false;
    m_config.logToFile = false;
    strcpy(m_config.logFilePath, "windy.log");

    // Network
    m_config.enableNetworkPatches = false;
    strcpy(m_config.networkIP, "192.168.1.2");
    strcpy(m_config.networkNetmask, "255.255.255.0");
    strcpy(m_config.networkGateway, "192.168.1.1");
    strcpy(m_config.or2IP, "");
    strcpy(m_config.or2Netmask, "");

    // Input defaults
    m_config.input.player1Start = KEY_1;
    m_config.input.player1Service = KEY_5;
    m_config.input.player1Coin = KEY_5;
    m_config.input.player1Up = KEY_UP;
    m_config.input.player1Down = KEY_DOWN;
    m_config.input.player1Left = KEY_LEFT;
    m_config.input.player1Right = KEY_RIGHT;
    m_config.input.player1Button1 = KEY_Z;
    m_config.input.player1Button2 = KEY_X;
    m_config.input.player1Button3 = KEY_C;
    m_config.input.player1Button4 = KEY_V;
    m_config.input.player1Button5 = KEY_B;
    m_config.input.player1Button6 = KEY_N;

    m_config.input.player2Start = KEY_2;
    m_config.input.player2Service = KEY_6;
    m_config.input.player2Coin = KEY_6;
    m_config.input.player2Up = KEY_I;
    m_config.input.player2Down = KEY_K;
    m_config.input.player2Left = KEY_J;
    m_config.input.player2Right = KEY_L;
    m_config.input.player2Button1 = KEY_A;
    m_config.input.player2Button2 = KEY_S;
    m_config.input.player2Button3 = KEY_D;
    m_config.input.player2Button4 = KEY_F;
    m_config.input.player2Button5 = KEY_G;
    m_config.input.player2Button6 = KEY_H;

    m_config.input.test = KEY_F1;
    m_config.input.service = KEY_F2;
    m_config.input.analogDeadzone = 8000;
    m_config.input.analogReverseX = false;
    m_config.input.analogReverseY = false;

    // Game detection
    m_config.gameId = GAME_UNKNOWN;
    strcpy(m_config.gameTitle, "Unknown Game");
    strcpy(m_config.gameDVP, "");
    m_config.gameNativeWidth = 640;
    m_config.gameNativeHeight = 480;
    m_config.gameType = DIGITAL;
    m_config.gameGroup = GROUP_NONE;
    m_config.gameLindberghColour = YELLOW;
}

bool ConfigManager::Load(const char *filename)
{
    if (!filename)
    {
        log_error("ConfigManager::Load: filename is null");
        return false;
    }

    IniParser parser;
    if (!parser.Load(filename))
    {
        log_warn("ConfigManager::Load: Cannot load '%s', generating defaults", filename);
        Save(filename);  // Generate default lindbergh.ini
        return false;
    }

    m_loadedPath = filename;

    // Helper lambda for string values (strips quotes)
    auto getStr = [&](const char* sec, const char* key, char* dest, size_t maxLen) {
        const char* val = parser.GetValue(sec, key);
        if (val && val[0]) {
            // Strip surrounding quotes if present
            std::string s = val;
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                s = s.substr(1, s.size() - 2);
            strncpy(dest, s.c_str(), maxLen - 1);
            dest[maxLen - 1] = '\0';
        }
    };

    // Helper for AUTO/true/false/none/int values (lindbergh-loader style)
    auto getIntAuto = [&](const char* sec, const char* key, int defaultVal) -> int {
        const char* val = parser.GetValue(sec, key);
        if (!val || !val[0]) return defaultVal;
        std::string s = val;
        // Trim whitespace
        while (!s.empty() && isspace(s.front())) s.erase(s.begin());
        while (!s.empty() && isspace(s.back())) s.pop_back();
        // Strip quotes
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            s = s.substr(1, s.size() - 2);
        // Case-insensitive compare
        std::string lower = s;
        for (auto& c : lower) c = (char)tolower(c);
        if (lower == "auto") return defaultVal;
        if (lower == "true") return 1;
        if (lower == "false") return 0;
        if (lower == "none") return -1;
        return atoi(s.c_str());
    };

    auto getFloatVal = [&](const char* sec, const char* key, float defaultVal) -> float {
        const char* val = parser.GetValue(sec, key);
        if (!val || !val[0]) return defaultVal;
        return (float)atof(val);
    };

    // [Display]
    m_config.width = getIntAuto("Display", "WIDTH", m_config.width);
    m_config.height = getIntAuto("Display", "HEIGHT", m_config.height);
    m_config.fullscreen = getIntAuto("Display", "FULLSCREEN", m_config.fullscreen) != 0;
    m_config.keepAspectRatio = getIntAuto("Display", "KEEP_ASPECT_RATIO", m_config.keepAspectRatio) != 0;

    // [Emulation]
    const char* regionRaw = parser.GetValue("Emulation", "REGION");
    if (regionRaw && regionRaw[0]) {
        std::string r = regionRaw;
        while (!r.empty() && isspace(r.front())) r.erase(r.begin());
        while (!r.empty() && isspace(r.back())) r.pop_back();
        if (r == "JP") m_config.region = REGION_JAPAN;
        else if (r == "US") m_config.region = REGION_US;
        else if (r == "EX") m_config.region = REGION_EXPORT;
    }
    int fp = getIntAuto("Emulation", "FREEPLAY", m_config.freeplay ? 1 : -1);
    m_config.freeplay = (fp == 1);
    m_config.emulateJVS = getIntAuto("Emulation", "EMULATE_JVS", m_config.emulateJVS) != 0;
    m_config.emulateRideboard = getIntAuto("Emulation", "EMULATE_RIDEBOARD", m_config.emulateRideboard) != 0;
    m_config.emulateDriveboard = getIntAuto("Emulation", "EMULATE_DRIVEBOARD", m_config.emulateDriveboard) != 0;
    m_config.emulateMotionboard = getIntAuto("Emulation", "EMULATE_MOTIONBOARD", m_config.emulateMotionboard) != 0;
    m_config.emulateCardReader = getIntAuto("Emulation", "EMULATE_ID_CARDREADER", m_config.emulateCardReader) != 0;
    if (!m_config.emulateCardReader)
        m_config.emulateCardReader = getIntAuto("Emulation", "EMULATE_HW210_CARDREADER", m_config.emulateCardReader) != 0;
    m_config.emulateTouchscreen = getIntAuto("Emulation", "EMULATE_TOUCHSCREEN", m_config.emulateTouchscreen) != 0;

    // [Paths]
    getStr("Paths", "SRAM_PATH", m_config.sramPath, CONFIG_PATH_MAX);
    getStr("Paths", "EEPROM_PATH", m_config.eepromPath, CONFIG_PATH_MAX);
    getStr("Paths", "JVS_PATH", m_config.jvsPath, CONFIG_PATH_MAX);

    // [Graphics]
    const char *fpsRaw = parser.GetValue("Graphics", "FPS_LIMIT");
    if (fpsRaw && fpsRaw[0])
    {
        m_config.fpsLimit = atof(fpsRaw);
        if (m_config.fpsLimit < 0.0)
            m_config.fpsLimit = 0.0;
    }
    else
    {
        // For backward compatibility.
        int oldLimiter = getIntAuto("Graphics", "FPS_LIMITER_ENABLED", 0);
        m_config.fpsLimit = oldLimiter ? 60.0 : 0.0;
    }
    m_config.vsync = getIntAuto("Graphics", "VSYNC", m_config.vsync) != 0;

    // [Network]
    m_config.enableNetworkPatches = getIntAuto("Network", "ENABLE_NETWORK_PATCHES", m_config.enableNetworkPatches) != 0;
    getStr("Network", "OR2_IPADDRESS", m_config.or2IP, 64);
    getStr("Network", "OR2_NETMASK", m_config.or2Netmask, 64);

    // [System]
    m_config.showDebugMessages = getIntAuto("System", "DEBUG_MSGS", m_config.showDebugMessages) != 0;
    m_config.logToFile = getIntAuto("System", "LOG_TO_FILE", m_config.logToFile) != 0;
    getStr("System", "LOG_FILE_PATH", m_config.logFilePath, CONFIG_PATH_MAX);

    const char* colourRaw = parser.GetValue("System", "LINDBERGH_COLOUR");
    if (colourRaw && colourRaw[0]) {
        std::string c = colourRaw;
        for (auto& ch : c) ch = (char)toupper(ch);
        while (!c.empty() && isspace(c.front())) c.erase(c.begin());
        while (!c.empty() && isspace(c.back())) c.pop_back();
        if (c == "RED") m_config.gameLindberghColour = RED;
        else if (c == "YELLOW") m_config.gameLindberghColour = YELLOW;
        else if (c == "REDEX") m_config.gameLindberghColour = REDEX;
    }

    log_info("ConfigManager: Loaded configuration from '%s'", filename);
    return true;
}

bool ConfigManager::Save(const char *filename)
{
    const char *savePath = filename ? filename : m_loadedPath.c_str();
    if (!savePath || savePath[0] == '\0')
        savePath = "lindbergh.ini";

    FILE* file = fopen(savePath, "w");
    if (!file)
    {
        log_error("ConfigManager::Save: Failed to open '%s' for writing", savePath);
        return false;
    }

    // [Display]
    fprintf(file, "[Display]\n");
    fprintf(file, "# Set the width resolution here\nWIDTH = %d\n\n", m_config.width);
    fprintf(file, "# Set the height resolution here\nHEIGHT = %d\n\n", m_config.height);
    fprintf(file, "# Set to true for full screen\nFULLSCREEN = %s\n\n", m_config.fullscreen ? "true" : "false");
    fprintf(file, "# Set to keep the aspect ratio\nKEEP_ASPECT_RATIO = %s\n\n", m_config.keepAspectRatio ? "true" : "false");

    // [Emulation]
    fprintf(file, "[Emulation]\n");
    const char* regionStr = "EX";
    if (m_config.region == REGION_JAPAN) regionStr = "JP";
    else if (m_config.region == REGION_US) regionStr = "US";
    else if (m_config.region == REGION_EXPORT) regionStr = "EX";
    fprintf(file, "# Set the Region (JP/US/EX)\nREGION = %s\n\n", regionStr);
    fprintf(file, "# Set to true for Free Play, none to leave as default\nFREEPLAY = %s\n\n",
            m_config.freeplay ? "true" : "none");
    fprintf(file, "# Set to true to emulate JVS\nEMULATE_JVS = %s\n\n", m_config.emulateJVS ? "true" : "false");
    fprintf(file, "# Set to true to emulate the rideboard\nEMULATE_RIDEBOARD = AUTO\n\n");
    fprintf(file, "# Set to true to emulate the driveboard\nEMULATE_DRIVEBOARD = AUTO\n\n");
    fprintf(file, "# Set to true to emulate the motion board\nEMULATE_MOTIONBOARD = AUTO\n\n");
    fprintf(file, "# Set to true to enable card reader emulation in VT3 or R-Tuned\nEMULATE_HW210_CARDREADER = AUTO\n\n");
    fprintf(file, "# Set to true to enable card reader emulation in ID4 and ID5\nEMULATE_ID_CARDREADER = AUTO\n\n");
    fprintf(file, "# Set to true to enable touchscreen emulation\nEMULATE_TOUCHSCREEN = AUTO\n\n");

    // [Paths]
    fprintf(file, "[Paths]\n");
    fprintf(file, "# Define the path to the sram.bin file\nSRAM_PATH = \"%s\"\n\n", m_config.sramPath);
    fprintf(file, "# Define the path to the eeprom.bin file\nEEPROM_PATH = \"%s\"\n\n", m_config.eepromPath);
    fprintf(file, "# Define the JVS serial path\nJVS_PATH = \"%s\"\n\n", m_config.jvsPath);

    // [Graphics]
    fprintf(file, "[Graphics]\n");
    fprintf(file, "# Set FPS limit (0.00 disables limiter)\nFPS_LIMIT = %.2f\n\n", m_config.fpsLimit);
    fprintf(file, "# Enable VSync\nVSYNC = %s\n\n", m_config.vsync ? "true" : "false");

    // [System]
    fprintf(file, "[System]\n");
    fprintf(file, "# Set to true to see debug messages in the console\nDEBUG_MSGS = %s\n\n",
            m_config.showDebugMessages ? "true" : "false");
    fprintf(file, "# Log to file\nLOG_TO_FILE = %s\n\n", m_config.logToFile ? "true" : "false");
    fprintf(file, "# Log file path\nLOG_FILE_PATH = \"%s\"\n\n", m_config.logFilePath);
    const char* colourStr = "YELLOW";
    if (m_config.gameLindberghColour == RED) colourStr = "RED";
    else if (m_config.gameLindberghColour == REDEX) colourStr = "REDEX";
    fprintf(file, "# Set the colour of the lindbergh (YELLOW, RED, REDEX)\nLINDBERGH_COLOUR = %s\n\n", colourStr);

    // [Network]
    fprintf(file, "[Network]\n");
    fprintf(file, "# Enable network patches\nENABLE_NETWORK_PATCHES = %s\n\n",
            m_config.enableNetworkPatches ? "true" : "false");
    fprintf(file, "# Outrun 2 IP and Netmask\nOR2_IPADDRESS = \"%s\"\nOR2_NETMASK = \"%s\"\n\n",
            m_config.or2IP, m_config.or2Netmask);

    fclose(file);
    m_loadedPath = savePath;

    log_info("ConfigManager: Saved configuration to '%s'", savePath);
    return true;
}

bool ConfigManager::DetectGame(uint32_t crc32)
{
    const GameDataEntry *entry = FindGameByCRC32(crc32);

    if (entry)
    {
        m_config.gameId = entry->enumId;
        strncpy(m_config.gameTitle, entry->gameTitle, 255);
        strncpy(m_config.gameDVP, entry->gameDVP, 31);
        m_config.gameNativeWidth = entry->width;
        m_config.gameNativeHeight = entry->height;
        m_config.gameType = entry->gameType;
        m_config.gameGroup = entry->gameGroup;
        m_config.gameLindberghColour = entry->colour;
        m_config.jvsIOType = entry->jvsType;

        if (entry->emulateRideboard)
            m_config.emulateRideboard = true;
        if (entry->emulateDriveboard)
            m_config.emulateDriveboard = true;
        if (entry->emulateMotionboard)
            m_config.emulateMotionboard = true;
        if (entry->emulateHW210CardReader || entry->emulateIDCardReader)
            m_config.emulateCardReader = true;
        if (entry->emulateTouchscreen)
            m_config.emulateTouchscreen = true;

        log_info("ConfigManager: Detected game '%s' (DVP: %s, CRC32: 0x%08X)", entry->gameTitle, entry->gameDVP, crc32);

        ApplyGameDefaults();
        return true;
    }

    log_warn("ConfigManager: Unknown game CRC: 0x%08X", crc32);
    return false;
}

void ConfigManager::ApplyGameDefaults()
{
    switch (m_config.gameGroup)
    {
        case GROUP_ABC:
            m_config.jvsIOType = SEGA_TYPE_1;
            break;
        case GROUP_OUTRUN:
            m_config.emulateDriveboard = true;
            m_config.emulateMotionboard = true;
            break;
        case GROUP_HOD4_SP:
        case GROUP_LGJ:
            m_config.emulateRideboard = true;
            break;
        case GROUP_ID4_JAP:
        case GROUP_ID4_EXP:
        case GROUP_ID5:
            m_config.emulateCardReader = true;
            break;
        default:
            break;
    }

    if (m_config.width == 1360 && m_config.height == 768)
    {
        if (m_config.gameNativeWidth > 0 && m_config.gameNativeHeight > 0)
        {
            m_config.width = m_config.gameNativeWidth;
            m_config.height = m_config.gameNativeHeight;
        }
    }
}

#define _CRT_SECURE_NO_WARNINGS

#include "config.h"
#include "gamedata.h"
#include "iniparser.h"
#include "log.h"

#include <cstring>
#include <cstdlib>

// SDL3 Key codes
#define KEY_1           30
#define KEY_2           31
#define KEY_5           34
#define KEY_6           35
#define KEY_A           4
#define KEY_B           5
#define KEY_C           6
#define KEY_D           7
#define KEY_F           9
#define KEY_G           10
#define KEY_H           11
#define KEY_I           12
#define KEY_J           13
#define KEY_K           14
#define KEY_L           15
#define KEY_N           17
#define KEY_S           22
#define KEY_V           25
#define KEY_X           27
#define KEY_Z           29
#define KEY_UP          82
#define KEY_DOWN        81
#define KEY_LEFT        80
#define KEY_RIGHT       79
#define KEY_F1          58
#define KEY_F2          59

// ============================================================
// ConfigManager Implementation
// ============================================================

ConfigManager& ConfigManager::Instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    InitDefaults();
}

ConfigManager::~ConfigManager() {
}

void ConfigManager::InitDefaults() {
    memset(&m_config, 0, sizeof(WindyConfig));
    
    // General
    strcpy(m_config.gamePath, "");
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
    m_config.fpsLimit = 0;
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

bool ConfigManager::Load(const char* filename) {
    if (!filename) {
        log_error("ConfigManager::Load: filename is null");
        return false;
    }
    
    IniParser parser;
    if (!parser.Load(filename)) {
        log_warn("ConfigManager::Load: Cannot load '%s', using defaults", filename);
        return false;
    }
    
    m_loadedPath = filename;
    
    // [General]
    if (parser.HasSection("General")) {
        const char* val = parser.GetValue("General", "GamePath");
        if (val[0]) strncpy(m_config.gamePath, val, CONFIG_PATH_MAX - 1);
        val = parser.GetValue("General", "EepromPath");
        if (val[0]) strncpy(m_config.eepromPath, val, CONFIG_PATH_MAX - 1);
        val = parser.GetValue("General", "SramPath");
        if (val[0]) strncpy(m_config.sramPath, val, CONFIG_PATH_MAX - 1);
    }
    
    // [Display]
    if (parser.HasSection("Display")) {
        m_config.width = parser.GetInt("Display", "Width", m_config.width);
        m_config.height = parser.GetInt("Display", "Height", m_config.height);
        m_config.fullscreen = parser.GetBool("Display", "Fullscreen", m_config.fullscreen);
        m_config.keepAspectRatio = parser.GetBool("Display", "KeepAspectRatio", m_config.keepAspectRatio);
        m_config.stretchToFit = parser.GetBool("Display", "StretchToFit", m_config.stretchToFit);
        m_config.windowX = parser.GetInt("Display", "WindowX", m_config.windowX);
        m_config.windowY = parser.GetInt("Display", "WindowY", m_config.windowY);
    }
    
    // [Emulation]
    if (parser.HasSection("Emulation")) {
        m_config.emulateJVS = parser.GetBool("Emulation", "EmulateJVS", m_config.emulateJVS);
        m_config.emulateRideboard = parser.GetBool("Emulation", "EmulateRideboard", m_config.emulateRideboard);
        m_config.emulateDriveboard = parser.GetBool("Emulation", "EmulateDriveboard", m_config.emulateDriveboard);
        m_config.emulateMotionboard = parser.GetBool("Emulation", "EmulateMotionboard", m_config.emulateMotionboard);
        m_config.emulateCardReader = parser.GetBool("Emulation", "EmulateCardReader", m_config.emulateCardReader);
        m_config.emulateTouchscreen = parser.GetBool("Emulation", "EmulateTouchscreen", m_config.emulateTouchscreen);
        int jvsType = parser.GetInt("Emulation", "JVSIOType", m_config.jvsIOType);
        m_config.jvsIOType = (jvsType == 0) ? SEGA_TYPE_1 : SEGA_TYPE_3;
        const char* jvsPath = parser.GetValue("Emulation", "JVSPath");
        if (jvsPath[0]) strncpy(m_config.jvsPath, jvsPath, CONFIG_PATH_MAX - 1);
    }
    
    // [Region]
    if (parser.HasSection("Region")) {
        int region = parser.GetInt("Region", "Region", m_config.region);
        if (region >= 0 && region <= 3) m_config.region = static_cast<Region>(region);
        m_config.freeplay = parser.GetBool("Region", "Freeplay", m_config.freeplay);
    }
    
    // [Graphics]
    if (parser.HasSection("Graphics")) {
        m_config.enableGPUSpoofing = parser.GetBool("Graphics", "EnableGPUSpoofing", m_config.enableGPUSpoofing);
        m_config.fpsLimit = parser.GetInt("Graphics", "FPSLimit", m_config.fpsLimit);
        m_config.vsync = parser.GetBool("Graphics", "VSync", m_config.vsync);
    }
    
    // [Debug]
    if (parser.HasSection("Debug")) {
        m_config.showDebugMessages = parser.GetBool("Debug", "ShowDebugMessages", m_config.showDebugMessages);
        m_config.logToFile = parser.GetBool("Debug", "LogToFile", m_config.logToFile);
        const char* logPath = parser.GetValue("Debug", "LogFilePath");
        if (logPath[0]) strncpy(m_config.logFilePath, logPath, CONFIG_PATH_MAX - 1);
    }
    
    // [Network]
    if (parser.HasSection("Network")) {
        m_config.enableNetworkPatches = parser.GetBool("Network", "EnableNetworkPatches", m_config.enableNetworkPatches);
        const char* ip = parser.GetValue("Network", "IP");
        if (ip[0]) strncpy(m_config.networkIP, ip, 63);
        const char* netmask = parser.GetValue("Network", "Netmask");
        if (netmask[0]) strncpy(m_config.networkNetmask, netmask, 63);
        const char* gateway = parser.GetValue("Network", "Gateway");
        if (gateway[0]) strncpy(m_config.networkGateway, gateway, 63);
        const char* or2ip = parser.GetValue("Network", "OR2_IP");
        if (or2ip[0]) strncpy(m_config.or2IP, or2ip, 63);
        const char* or2mask = parser.GetValue("Network", "OR2_Netmask");
        if (or2mask[0]) strncpy(m_config.or2Netmask, or2mask, 63);
    }
    
    // [Input]
    if (parser.HasSection("Input")) {
        m_config.input.player1Start = parser.GetInt("Input", "P1_Start", m_config.input.player1Start);
        m_config.input.player1Coin = parser.GetInt("Input", "P1_Coin", m_config.input.player1Coin);
        m_config.input.player1Up = parser.GetInt("Input", "P1_Up", m_config.input.player1Up);
        m_config.input.player1Down = parser.GetInt("Input", "P1_Down", m_config.input.player1Down);
        m_config.input.player1Left = parser.GetInt("Input", "P1_Left", m_config.input.player1Left);
        m_config.input.player1Right = parser.GetInt("Input", "P1_Right", m_config.input.player1Right);
        m_config.input.player1Button1 = parser.GetInt("Input", "P1_Button1", m_config.input.player1Button1);
        m_config.input.player1Button2 = parser.GetInt("Input", "P1_Button2", m_config.input.player1Button2);
        m_config.input.player1Button3 = parser.GetInt("Input", "P1_Button3", m_config.input.player1Button3);
        m_config.input.player1Button4 = parser.GetInt("Input", "P1_Button4", m_config.input.player1Button4);
        m_config.input.player1Button5 = parser.GetInt("Input", "P1_Button5", m_config.input.player1Button5);
        m_config.input.player1Button6 = parser.GetInt("Input", "P1_Button6", m_config.input.player1Button6);
        m_config.input.player2Start = parser.GetInt("Input", "P2_Start", m_config.input.player2Start);
        m_config.input.player2Coin = parser.GetInt("Input", "P2_Coin", m_config.input.player2Coin);
        m_config.input.player2Up = parser.GetInt("Input", "P2_Up", m_config.input.player2Up);
        m_config.input.player2Down = parser.GetInt("Input", "P2_Down", m_config.input.player2Down);
        m_config.input.player2Left = parser.GetInt("Input", "P2_Left", m_config.input.player2Left);
        m_config.input.player2Right = parser.GetInt("Input", "P2_Right", m_config.input.player2Right);
        m_config.input.player2Button1 = parser.GetInt("Input", "P2_Button1", m_config.input.player2Button1);
        m_config.input.player2Button2 = parser.GetInt("Input", "P2_Button2", m_config.input.player2Button2);
        m_config.input.player2Button3 = parser.GetInt("Input", "P2_Button3", m_config.input.player2Button3);
        m_config.input.player2Button4 = parser.GetInt("Input", "P2_Button4", m_config.input.player2Button4);
        m_config.input.player2Button5 = parser.GetInt("Input", "P2_Button5", m_config.input.player2Button5);
        m_config.input.player2Button6 = parser.GetInt("Input", "P2_Button6", m_config.input.player2Button6);
        m_config.input.test = parser.GetInt("Input", "Test", m_config.input.test);
        m_config.input.service = parser.GetInt("Input", "Service", m_config.input.service);
        m_config.input.analogDeadzone = parser.GetInt("Input", "AnalogDeadzone", m_config.input.analogDeadzone);
        m_config.input.analogReverseX = parser.GetBool("Input", "AnalogReverseX", m_config.input.analogReverseX);
        m_config.input.analogReverseY = parser.GetBool("Input", "AnalogReverseY", m_config.input.analogReverseY);
    }
    
    log_info("ConfigManager: Loaded configuration from '%s'", filename);
    return true;
}

bool ConfigManager::Save(const char* filename) {
    const char* savePath = filename ? filename : m_loadedPath.c_str();
    if (!savePath || savePath[0] == '\0') savePath = "windy.ini";
    
    IniParser parser;
    
    parser.SetValue("General", "GamePath", m_config.gamePath);
    parser.SetValue("General", "EepromPath", m_config.eepromPath);
    parser.SetValue("General", "SramPath", m_config.sramPath);
    
    parser.SetInt("Display", "Width", m_config.width);
    parser.SetInt("Display", "Height", m_config.height);
    parser.SetBool("Display", "Fullscreen", m_config.fullscreen);
    parser.SetBool("Display", "KeepAspectRatio", m_config.keepAspectRatio);
    parser.SetBool("Display", "StretchToFit", m_config.stretchToFit);
    parser.SetInt("Display", "WindowX", m_config.windowX);
    parser.SetInt("Display", "WindowY", m_config.windowY);
    
    parser.SetBool("Emulation", "EmulateJVS", m_config.emulateJVS);
    parser.SetBool("Emulation", "EmulateRideboard", m_config.emulateRideboard);
    parser.SetBool("Emulation", "EmulateDriveboard", m_config.emulateDriveboard);
    parser.SetBool("Emulation", "EmulateMotionboard", m_config.emulateMotionboard);
    parser.SetBool("Emulation", "EmulateCardReader", m_config.emulateCardReader);
    parser.SetBool("Emulation", "EmulateTouchscreen", m_config.emulateTouchscreen);
    parser.SetInt("Emulation", "JVSIOType", m_config.jvsIOType);
    parser.SetValue("Emulation", "JVSPath", m_config.jvsPath);
    
    parser.SetInt("Region", "Region", m_config.region);
    parser.SetBool("Region", "Freeplay", m_config.freeplay);
    
    parser.SetBool("Graphics", "EnableGPUSpoofing", m_config.enableGPUSpoofing);
    parser.SetInt("Graphics", "FPSLimit", m_config.fpsLimit);
    parser.SetBool("Graphics", "VSync", m_config.vsync);
    
    parser.SetBool("Debug", "ShowDebugMessages", m_config.showDebugMessages);
    parser.SetBool("Debug", "LogToFile", m_config.logToFile);
    parser.SetValue("Debug", "LogFilePath", m_config.logFilePath);
    
    parser.SetBool("Network", "EnableNetworkPatches", m_config.enableNetworkPatches);
    parser.SetValue("Network", "IP", m_config.networkIP);
    parser.SetValue("Network", "Netmask", m_config.networkNetmask);
    parser.SetValue("Network", "Gateway", m_config.networkGateway);
    parser.SetValue("Network", "OR2_IP", m_config.or2IP);
    parser.SetValue("Network", "OR2_Netmask", m_config.or2Netmask);
    
    parser.SetInt("Input", "P1_Start", m_config.input.player1Start);
    parser.SetInt("Input", "P1_Coin", m_config.input.player1Coin);
    parser.SetInt("Input", "P1_Up", m_config.input.player1Up);
    parser.SetInt("Input", "P1_Down", m_config.input.player1Down);
    parser.SetInt("Input", "P1_Left", m_config.input.player1Left);
    parser.SetInt("Input", "P1_Right", m_config.input.player1Right);
    parser.SetInt("Input", "P1_Button1", m_config.input.player1Button1);
    parser.SetInt("Input", "P1_Button2", m_config.input.player1Button2);
    parser.SetInt("Input", "P1_Button3", m_config.input.player1Button3);
    parser.SetInt("Input", "P1_Button4", m_config.input.player1Button4);
    parser.SetInt("Input", "P1_Button5", m_config.input.player1Button5);
    parser.SetInt("Input", "P1_Button6", m_config.input.player1Button6);
    parser.SetInt("Input", "P2_Start", m_config.input.player2Start);
    parser.SetInt("Input", "P2_Coin", m_config.input.player2Coin);
    parser.SetInt("Input", "P2_Up", m_config.input.player2Up);
    parser.SetInt("Input", "P2_Down", m_config.input.player2Down);
    parser.SetInt("Input", "P2_Left", m_config.input.player2Left);
    parser.SetInt("Input", "P2_Right", m_config.input.player2Right);
    parser.SetInt("Input", "P2_Button1", m_config.input.player2Button1);
    parser.SetInt("Input", "P2_Button2", m_config.input.player2Button2);
    parser.SetInt("Input", "P2_Button3", m_config.input.player2Button3);
    parser.SetInt("Input", "P2_Button4", m_config.input.player2Button4);
    parser.SetInt("Input", "P2_Button5", m_config.input.player2Button5);
    parser.SetInt("Input", "P2_Button6", m_config.input.player2Button6);
    parser.SetInt("Input", "Test", m_config.input.test);
    parser.SetInt("Input", "Service", m_config.input.service);
    parser.SetInt("Input", "AnalogDeadzone", m_config.input.analogDeadzone);
    parser.SetBool("Input", "AnalogReverseX", m_config.input.analogReverseX);
    parser.SetBool("Input", "AnalogReverseY", m_config.input.analogReverseY);
    
    if (!parser.Save(savePath)) {
        log_error("ConfigManager::Save: Failed to save to '%s'", savePath);
        return false;
    }
    
    log_info("ConfigManager: Saved configuration to '%s'", savePath);
    return true;
}

bool ConfigManager::DetectGame(uint32_t crc32) {
    const GameDataEntry* entry = FindGameByCRC32(crc32);
    
    if (entry) {
        m_config.gameId = entry->enumId;
        strncpy(m_config.gameTitle, entry->gameTitle, 255);
        strncpy(m_config.gameDVP, entry->gameDVP, 31);
        m_config.gameNativeWidth = entry->width;
        m_config.gameNativeHeight = entry->height;
        m_config.gameType = entry->gameType;
        m_config.gameGroup = entry->gameGroup;
        m_config.gameLindberghColour = entry->colour;
        m_config.jvsIOType = entry->jvsType;
        
        if (entry->emulateRideboard) m_config.emulateRideboard = true;
        if (entry->emulateDriveboard) m_config.emulateDriveboard = true;
        if (entry->emulateMotionboard) m_config.emulateMotionboard = true;
        if (entry->emulateHW210CardReader || entry->emulateIDCardReader) m_config.emulateCardReader = true;
        if (entry->emulateTouchscreen) m_config.emulateTouchscreen = true;
        
        log_info("ConfigManager: Detected game '%s' (DVP: %s, CRC32: 0x%08X)", 
            entry->gameTitle, entry->gameDVP, crc32);
        
        ApplyGameDefaults();
        return true;
    }
    
    log_warn("ConfigManager: Unknown game CRC: 0x%08X", crc32);
    return false;
}

void ConfigManager::ApplyGameDefaults() {
    switch (m_config.gameGroup) {
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
    
    if (m_config.width == 1360 && m_config.height == 768) {
        if (m_config.gameNativeWidth > 0 && m_config.gameNativeHeight > 0) {
            m_config.width = m_config.gameNativeWidth;
            m_config.height = m_config.gameNativeHeight;
        }
    }
}

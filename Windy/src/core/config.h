#pragma once

#include <stdint.h>
#include <string>

// ============================================================
// Avoid Windows macro conflicts
// ============================================================
#ifdef ABC
#undef ABC
#endif

// ============================================================
// Maximum path length (Windows compatible)
// ============================================================
#ifdef _WIN32
#include <windows.h>
#define CONFIG_PATH_MAX MAX_PATH
#else
#include <limits.h>
#define CONFIG_PATH_MAX PATH_MAX
#endif

// ============================================================
// Game Identification Enums
// ============================================================

enum GameID {
    GAME_UNKNOWN = 0,

    // After Burner Climax
    AFTER_BURNER_CLIMAX,
    AFTER_BURNER_CLIMAX_REVA,
    AFTER_BURNER_CLIMAX_REVB,
    AFTER_BURNER_CLIMAX_SDX,
    AFTER_BURNER_CLIMAX_SDX_REVA,
    AFTER_BURNER_CLIMAX_SE,
    AFTER_BURNER_CLIMAX_SE_REVA,

    // Ghost Squad Evolution
    GHOST_SQUAD_EVOLUTION,

    // Harley Davidson
    HARLEY_DAVIDSON,

    // Hummer
    HUMMER,
    HUMMER_SDLX,
    HUMMER_EXTREME,
    HUMMER_EXTREME_MDX,

    // Initial D
    INITIALD_4_REVA,
    INITIALD_4_REVB,
    INITIALD_4_REVC,
    INITIALD_4_REVD,
    INITIALD_4_REVG,
    INITIALD_4_EXP_REVB,
    INITIALD_4_EXP_REVC,
    INITIALD_4_EXP_REVD,
    INITIALD_5_JAP_REVA,
    INITIALD_5_JAP_REVC,
    INITIALD_5_JAP_REVF,
    INITIALD_5_JAP_SBQZ_SERVERBOX,
    INITIALD_5_EXP,
    INITIALD_5_EXP_20,
    INITIALD_5_EXP_20A,

    // Let's Go Jungle
    LETS_GO_JUNGLE,
    LETS_GO_JUNGLE_REVA,
    LETS_GO_JUNGLE_SPECIAL,

    // Mahjong
    MJ4_REVG,
    MJ4_EVO,

    // Outrun 2
    OUTRUN_2_SP_SDX,
    OUTRUN_2_SP_SDX_REVA,
    OUTRUN_2_SP_SDX_TEST,
    OUTRUN_2_SP_SDX_REVA_TEST,
    OUTRUN_2_SP_SDX_REVA_TEST2,

    // Primeval Hunt
    PRIMEVAL_HUNT,

    // Quiz Answer x Answer
    QUIZ_AXA,
    QUIZ_AXA_LIVE,

    // Rambo
    RAMBO,
    RAMBO_CHINA,

    // R-Tuned
    R_TUNED,

    // Segaboot
    SEGABOOT_2_4,
    SEGABOOT_2_4_SYM,

    // Sega Race TV
    SEGA_RACE_TV,

    // House of the Dead
    THE_HOUSE_OF_THE_DEAD_4_REVA,
    THE_HOUSE_OF_THE_DEAD_4_REVA_TEST,
    THE_HOUSE_OF_THE_DEAD_4_REVB,
    THE_HOUSE_OF_THE_DEAD_4_REVB_TEST,
    THE_HOUSE_OF_THE_DEAD_4_REVC,
    THE_HOUSE_OF_THE_DEAD_4_REVC_TEST,
    THE_HOUSE_OF_THE_DEAD_4_SPECIAL,
    THE_HOUSE_OF_THE_DEAD_4_SPECIAL_TEST,
    THE_HOUSE_OF_THE_DEAD_4_SPECIAL_REVB,
    THE_HOUSE_OF_THE_DEAD_4_SPECIAL_REVB_TEST,
    THE_HOUSE_OF_THE_DEAD_EX,
    THE_HOUSE_OF_THE_DEAD_EX_TEST,

    // Too Spicy
    TOO_SPICY,
    TOO_SPICY_TEST,

    // Virtua Fighter 5
    VIRTUA_FIGHTER_5,
    VIRTUA_FIGHTER_5_REVA,
    VIRTUA_FIGHTER_5_REVB,
    VIRTUA_FIGHTER_5_REVC,
    VIRTUA_FIGHTER_5_REVE,
    VIRTUA_FIGHTER_5_EXPORT,
    VIRTUA_FIGHTER_5_R,
    VIRTUA_FIGHTER_5_R_REVD,
    VIRTUA_FIGHTER_5_R_REVG,
    VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_REVA,
    VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_REVB,
    VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_REVB_6000,

    // Virtua Tennis 3
    VIRTUA_TENNIS_3,
    VIRTUA_TENNIS_3_TEST,
    VIRTUA_TENNIS_3_REVA,
    VIRTUA_TENNIS_3_REVA_TEST,
    VIRTUA_TENNIS_3_REVB,
    VIRTUA_TENNIS_3_REVB_TEST,
    VIRTUA_TENNIS_3_REVC,
    VIRTUA_TENNIS_3_REVC_TEST,

    // Other
    SKYCURSER,

    GAME_ID_COUNT
};

enum GameStatus {
    NOT_WORKING = 0,
    WORKING = 1
};

// JVS I/O Types - Use unique names to avoid conflicts with jvsboard.h
enum JVSIOType {
    SEGA_TYPE_1 = 0,  // After Burner Climax: 13 switches, 8-bit analog
    SEGA_TYPE_3 = 1   // Standard Lindbergh: 14 switches, 10-bit analog
};

enum GameType {
    SHOOTING = 0,
    DRIVING = 1,
    DIGITAL = 2,
    MAHJONG = 3,
    GAME_TYPE_ABC = 4  // After Burner Climax specific (renamed from ABC to avoid Windows conflict)
};

enum GameGroup {
    GROUP_NONE = -1,
    GROUP_ABC = 0,
    GROUP_HOD4,
    GROUP_HOD4_TEST,
    GROUP_HOD4_SP,
    GROUP_HOD4_SP_TEST,
    GROUP_LGJ,
    GROUP_OUTRUN,
    GROUP_OUTRUN_TEST,
    GROUP_VF5,
    GROUP_VT3,
    GROUP_VT3_TEST,
    GROUP_ID4_JAP,
    GROUP_ID4_EXP,
    GROUP_ID5,
    GROUP_ID_SERVERBOX,
    GROUP_HUMMER,
    GROUP_RAMBO
};

enum Colour {
    RED = 0,
    YELLOW = 1,
    REDEX = 2  // Red Extended
};

enum Region {
    REGION_JAPAN = 0,
    REGION_US = 1,
    REGION_EXPORT = 2,
    REGION_CHINA = 3
};

// ============================================================
// Input Mapping Structure
// ============================================================

struct InputMapping {
    // Player controls
    int player1Start;
    int player1Service;
    int player1Coin;
    int player1Up;
    int player1Down;
    int player1Left;
    int player1Right;
    int player1Button1;
    int player1Button2;
    int player1Button3;
    int player1Button4;
    int player1Button5;
    int player1Button6;

    int player2Start;
    int player2Service;
    int player2Coin;
    int player2Up;
    int player2Down;
    int player2Left;
    int player2Right;
    int player2Button1;
    int player2Button2;
    int player2Button3;
    int player2Button4;
    int player2Button5;
    int player2Button6;

    // System controls
    int test;
    int service;

    // Analog axes (for driving games, etc.)
    int analogDeadzone;
    bool analogReverseX;
    bool analogReverseY;
};

// ============================================================
// Configuration Structure
// ============================================================

struct WindyConfig {
    // --- General Settings ---
    char gamePath[CONFIG_PATH_MAX];
    char eepromPath[CONFIG_PATH_MAX];
    char sramPath[CONFIG_PATH_MAX];

    // --- Display Settings ---
    int width;
    int height;
    bool fullscreen;
    bool keepAspectRatio;
    bool stretchToFit;
    int windowX;
    int windowY;

    // --- Emulation Settings ---
    bool emulateJVS;
    bool emulateRideboard;
    bool emulateDriveboard;
    bool emulateMotionboard;
    bool emulateCardReader;
    bool emulateTouchscreen;

    JVSIOType jvsIOType;
    char jvsPath[CONFIG_PATH_MAX];  // For serial passthrough

    // --- Region & Game Settings ---
    Region region;
    bool freeplay;

    // --- Graphics Settings ---
    bool enableGPUSpoofing;
    double fpsLimit;
    bool vsync;

    // --- Debug Settings ---
    bool showDebugMessages;
    bool logToFile;
    char logFilePath[CONFIG_PATH_MAX];

    // --- Network Settings ---
    bool enableNetworkPatches;
    char networkIP[64];
    char networkNetmask[64];
    char networkGateway[64];

    // Outrun 2 specific
    char or2IP[64];
    char or2Netmask[64];

    // --- Input Settings ---
    InputMapping input;

    // --- Game Detection (filled automatically) ---
    GameID gameId;
    char gameTitle[256];
    char elfPath[CONFIG_PATH_MAX];
    char gameDVP[32];
    int gameNativeWidth;
    int gameNativeHeight;
    GameType gameType;
    GameGroup gameGroup;
    Colour gameLindberghColour;
};

// ============================================================
// Config Manager Class
// ============================================================

class IniParser;  // Forward declaration

class ConfigManager {
public:
    static ConfigManager& Instance();

    /**
     * @brief Initialize configuration with default values
     */
    void InitDefaults();

    /**
     * @brief Load configuration from INI file
     * @param filename Path to INI file (default: "lindbergh.ini")
     * @return true if successful
     */
    bool Load(const char* filename = "lindbergh.ini");

    /**
     * @brief Save configuration to INI file
     * @param filename Path to INI file (uses loaded path if nullptr)
     * @return true if successful
     */
    bool Save(const char* filename = nullptr);

    /**
     * @brief Detect game from ELF CRC32
     * @param crc32 CRC32 of the ELF file
     * @return true if game was detected
     */
    bool DetectGame(uint32_t crc32);

    /**
     * @brief Get the current configuration (read-only)
     */
    const WindyConfig& GetConfig() const { return m_config; }

    /**
     * @brief Get the current configuration (modifiable)
     */
    WindyConfig& GetConfig() { return m_config; }

    /**
     * @brief Get configuration pointer
     */
    WindyConfig* GetConfigPtr() { return &m_config; }

    void SetElfPath(const char *path)
    {
        strncpy_s(m_config.elfPath, path, CONFIG_PATH_MAX - 1);
    }
    const char *GetElfPath() const
    {
        return m_config.elfPath;
    }

  private:
    ConfigManager();
    ~ConfigManager();

    // Prevent copying
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void ParseSection(IniParser& parser, const char* section);
    void ApplyGameDefaults();

    WindyConfig m_config;
    std::string m_loadedPath;
};

// ============================================================
// Global Access Function (for convenience)
// ============================================================

/**
 * @brief Get the global configuration
 * @return Pointer to the configuration structure
 */
inline WindyConfig* getConfig() {
    return ConfigManager::Instance().GetConfigPtr();
}
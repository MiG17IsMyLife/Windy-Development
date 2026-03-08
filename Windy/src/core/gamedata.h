#pragma once

#include "config.h"
#include <stdint.h>

/**
 * @brief Game data entry structure
 *
 * Contains all information about a specific game version including
 * CRC32 for identification and hardware requirements.
 */
struct GameDataEntry {
    uint32_t crc32;
    const char* gameTitle;
    const char* gameDVP;
    const char* gameID;          // 4-character SEGA game ID
    GameID enumId;
    GameStatus status;
    JVSIOType jvsType;
    GameType gameType;
    int width;
    int height;
    GameGroup gameGroup;
    bool emulateRideboard;
    bool emulateDriveboard;
    bool emulateMotionboard;
    bool emulateHW210CardReader;
    bool emulateIDCardReader;
    bool emulateTouchscreen;
    Colour colour;
};

/**
 * @brief Complete game database with CRC32 values from Lindbergh-loader
 *
 * CRC32 values are computed from the main game executable.
 * This table is used for automatic game detection.
 */
inline const GameDataEntry* GetGameDatabase() {
    static const GameDataEntry database[] = {
        // ========================================
        // After Burner Climax
        // ========================================
        {0x2C8F5D57, "After Burner Climax", "DVP-0009", "SBLR", AFTER_BURNER_CLIMAX,
         NOT_WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0x13D90755, "After Burner Climax Rev A", "DVP-0009A", "SBLR", AFTER_BURNER_CLIMAX_REVA,
         NOT_WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0x633AD6FB, "After Burner Climax Rev B", "DVP-0009B", "SBLR", AFTER_BURNER_CLIMAX_REVB,
         NOT_WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0xCC32DEAE, "After Burner Climax SDX", "DVP-0018", "SBMN", AFTER_BURNER_CLIMAX_SDX,
         NOT_WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0x17114BC1, "After Burner Climax SDX Rev A", "DVP-0018A", "SBMN", AFTER_BURNER_CLIMAX_SDX_REVA,
         NOT_WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0x8BDD31BA, "After Burner Climax SE", "DVP-0031", "SBPB", AFTER_BURNER_CLIMAX_SE,
         NOT_WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0x3DF37873, "After Burner Climax SE Rev A", "DVP-0031A", "SBPB", AFTER_BURNER_CLIMAX_SE_REVA,
         NOT_WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        // ========================================
        // Ghost Squad Evolution
        // ========================================
        {0xAA19F0C1, "Ghost Squad Evolution", "DVP-0029A", "SBNJ", GHOST_SQUAD_EVOLUTION,
         NOT_WORKING, SEGA_TYPE_1, SHOOTING, 640, 480, GROUP_NONE,
         false, false, false, false, false, false, RED},

        // ========================================
        // Harley-Davidson
        // ========================================
        {0x75B48E22, "Harley-Davidson: King of the Road", "DVP-5007", "SBRG", HARLEY_DAVIDSON,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_NONE,
         false, false, false, false, false, false, RED},

        // ========================================
        // Hummer
        // ========================================
        {0x04D88552, "Hummer", "DVP-0057B", "SBQN", HUMMER,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_HUMMER,
         false, false, false, false, false, false, YELLOW},

        {0x653BC83B, "Hummer SDLX", "DVP-0057", "SBQN", HUMMER_SDLX,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_HUMMER,
         false, false, false, false, false, false, YELLOW},

        {0x05647A8E, "Hummer Extreme", "DVP-0079", "SBST", HUMMER_EXTREME,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_HUMMER,
         false, false, false, false, false, false, YELLOW},

        {0x4442EA15, "Hummer Extreme MDX", "DVP-0083", "SBST", HUMMER_EXTREME_MDX,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_HUMMER,
         false, false, false, false, false, false, YELLOW},

        // ========================================
        // Initial D Arcade Stage 4
        // ========================================
        {0x22905D60, "Initial D Arcade Stage 4 Rev A", "DVP-0019A", "SBML", INITIALD_4_REVA,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_JAP,
         false, false, false, false, true, false, YELLOW},

        {0x43582D48, "Initial D Arcade Stage 4 Rev B", "DVP-0019B", "SBML", INITIALD_4_REVB,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_JAP,
         false, false, false, false, true, false, YELLOW},

        {0x2D2A18C1, "Initial D Arcade Stage 4 Rev C", "DVP-0019C", "SBML", INITIALD_4_REVC,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_JAP,
         false, false, false, false, true, false, YELLOW},

        {0x9BFD0D98, "Initial D Arcade Stage 4 Rev D", "DVP-0019D", "SBML", INITIALD_4_REVD,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_JAP,
         false, false, false, false, true, false, YELLOW},

        {0x9CF9BBCC, "Initial D Arcade Stage 4 Rev G", "DVP-0019G", "SBML", INITIALD_4_REVG,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_JAP,
         false, false, false, false, true, false, YELLOW},

        {0xC345E213, "Initial D4 Export Rev B", "DVP-0030B", "SBNK", INITIALD_4_EXP_REVB,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_EXP,
         false, false, false, false, true, false, YELLOW},

        {0x98E6A516, "Initial D4 Export Rev C", "DVP-0030C", "SBNK", INITIALD_4_EXP_REVC,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_EXP,
         false, false, false, false, true, false, YELLOW},

        {0xF67365C9, "Initial D4 Export Rev D", "DVP-0030D", "SBNK", INITIALD_4_EXP_REVD,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_EXP,
         false, false, false, false, true, false, YELLOW},

        // ========================================
        // Initial D Arcade Stage 5
        // ========================================
        {0xE4F202BB, "Initial D Arcade Stage 5 Rev A", "DVP-0070A", "SBQZ", INITIALD_5_JAP_REVA,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
         false, false, false, false, true, false, YELLOW},

        {0x400C09CD, "Initial D Arcade Stage 5 Rev C", "DVP-0070C", "SBQZ", INITIALD_5_JAP_REVC,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
         false, false, false, false, true, false, YELLOW},

        {0x2E6732A3, "Initial D Arcade Stage 5 Rev F", "DVP-0070F", "SBQZ", INITIALD_5_JAP_REVF,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
         false, false, false, false, true, false, YELLOW},

        {0xAEEE6BEF, "Initial D Arcade Stage 5 ServerBox", "DVP-0070", "SBQZ", INITIALD_5_JAP_SBQZ_SERVERBOX,
         WORKING, SEGA_TYPE_3, DRIVING, 640, 480, GROUP_ID_SERVERBOX   ,
         false, false, false, false, false, false, YELLOW},

        {0xF99A3CDB, "Initial D5 EXP", "DVP-0075", "SBRY", INITIALD_5_EXP,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
         false, false, false, false, true, false, YELLOW},

        {0x8DF6BBF9, "Initial D5 EXP 2.0", "DVP-0084", "SBTS", INITIALD_5_EXP_20,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
         false, false, false, false, true, false, YELLOW},

        {0x2AF8004E, "Initial D5 EXP 2.0A", "DVP-0084A", "SBTS", INITIALD_5_EXP_20A,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
         false, false, false, false, true, false, YELLOW},

        // ========================================
        // Let's Go Jungle
        // ========================================
        {0x04E08C99, "Let's Go Jungle!", "DVP-0011", "SBLU", LETS_GO_JUNGLE,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1360, 768, GROUP_LGJ,
         false, false, false, false, false, false, YELLOW},

        {0x0C3D3CC3, "Let's Go Jungle! Rev A", "DVP-0011A", "SBLU", LETS_GO_JUNGLE_REVA,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1360, 768, GROUP_LGJ,
         false, false, false, false, false, false, YELLOW},

        {0xDD8BB792, "Let's Go Jungle Special", "DVP-0036A", "SBNU", LETS_GO_JUNGLE_SPECIAL,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1024, 768, GROUP_LGJ,
         true, false, false, false, false, false, YELLOW},

        // ========================================
        // Mahjong
        // ========================================
        {0x65489691, "Mj4 Rev G", "DVP-0049G", "SBPF", MJ4_REVG,
         NOT_WORKING, SEGA_TYPE_3, MAHJONG, 1360, 768, GROUP_NONE,
         false, false, false, false, false, true, YELLOW},

        {0x0AD7CF0F, "Mj4 Evolution", "DVP-0081C", "SBRV", MJ4_EVO,
         NOT_WORKING, SEGA_TYPE_3, MAHJONG, 1360, 768, GROUP_NONE,
         false, false, false, false, false, true, YELLOW},

        // ========================================
        // OutRun 2 SP SDX
        // ========================================
        {0x821C3404, "Outrun 2 SP SDX", "DVP-0015", "SBMB", OUTRUN_2_SP_SDX,
         WORKING, SEGA_TYPE_3, DRIVING, 800, 480, GROUP_OUTRUN,
         false, true, true, false, false, false, YELLOW},

        {0xB2CE9B23, "Outrun 2 SP SDX Rev A", "DVP-0015A", "SBMB", OUTRUN_2_SP_SDX_REVA,
         WORKING, SEGA_TYPE_3, DRIVING, 800, 480, GROUP_OUTRUN,
         false, true, true, false, false, false, YELLOW},

        {0xD9660B2E, "Outrun 2 SP SDX Test", "DVP-0015", "SBMB", OUTRUN_2_SP_SDX_TEST,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 800, 480, GROUP_OUTRUN_TEST,
         false, true, true, false, false, false, YELLOW},

        {0x13AF8581, "Outrun 2 SP SDX Rev A Test", "DVP-0015A", "SBMB", OUTRUN_2_SP_SDX_REVA_TEST,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 800, 480, GROUP_OUTRUN_TEST,
         false, true, true, false, false, false, YELLOW},

        // ========================================
        // Primeval Hunt
        // ========================================
        {0x4143F6B4, "Primeval Hunt", "DVP-0048A", "SBPP", PRIMEVAL_HUNT,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1360, 768, GROUP_NONE,
         false, false, false, false, false, false, YELLOW},

        // ========================================
        // Quiz Answer x Answer
        // ========================================
        {0xDCAD8ABA, "Quiz Answer x Answer", "DVP-0025H", "SBMS", QUIZ_AXA,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_NONE,
         false, false, false, false, false, true, YELLOW},

        {0xCB663DD0, "Quiz Answer x Answer Live", "DVP-0087D", "SBMS", QUIZ_AXA_LIVE,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_NONE,
         false, false, false, false, false, true, YELLOW},

        // ========================================
        // Rambo
        // ========================================
        {0x81E02850, "Rambo", "DVP-0069", "SBNJ", RAMBO,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 640, 480, GROUP_RAMBO,
         false, false, false, false, false, false, REDEX},

        {0xDCC1f8E7, "Rambo China", "DVP-0078", "SBQQ", RAMBO_CHINA,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 640, 480, GROUP_RAMBO,
         false, false, false, false, false, false, REDEX},

        // ========================================
        // R-Tuned
        // ========================================
        {0x089D6051, "R-Tuned Ultimate Street Racing", "DVP-0060", "SBQW", R_TUNED,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_NONE,
         false, false, false, false, false, false, YELLOW},

        // ========================================
        // Segaboot
        // ========================================
        {0xBEF9F7DC, "Segaboot 2.4", "DVP-0000", "AAAA", SEGABOOT_2_4,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 640, 480, GROUP_NONE,
         false, false, false, false, false, false, YELLOW},

        {0x0A4D1765, "Segaboot 2.4 (symlink)", "DVP-0000", "AAAA", SEGABOOT_2_4_SYM,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 640, 480, GROUP_NONE,
         false, false, false, false, false, false, YELLOW},

        // ========================================
        // Sega Race TV
        // ========================================
        {0xF99E5635, "Sega Race TV", "DVP-0044", "SBPD", SEGA_RACE_TV,
         NOT_WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_NONE,
         false, false, false, false, false, false, YELLOW},

        // ========================================
        // The House of the Dead 4
        // ========================================
        {0x51C4D2F6, "The House of the Dead 4 Rev A", "DVP-0003A", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVA,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4,
         false, false, false, false, false, false, YELLOW},

        {0x1348BCA8, "The House of the Dead 4 Rev A Test", "DVP-0003A", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVA_TEST,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4_TEST,
         false, false, false, false, false, false, YELLOW},

        {0x0AAE384E, "The House of the Dead 4 Rev B", "DVP-0003B", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVB,
         WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4,
         false, false, false, false, false, false, YELLOW},

        {0x352AA797, "The House of the Dead 4 Rev B Test", "DVP-0003B", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVB_TEST,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4_TEST,
         false, false, false, false, false, false, YELLOW},

        {0x42EED61A, "The House of the Dead 4 Rev C", "DVP-0003C", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVC,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4,
         false, false, false, false, false, false, YELLOW},

        {0x6DA6E511, "The House of the Dead 4 Rev C Test", "DVP-0003C", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVC_TEST,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4_TEST,
         false, false, false, false, false, false, YELLOW},

        {0xD39825A8, "The House of the Dead 4 Special", "DVP-0010", "SBLS", THE_HOUSE_OF_THE_DEAD_4_SPECIAL,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1024, 768, GROUP_HOD4_SP,
         true, false, false, false, false, false, YELLOW},

        {0x0745CF0A, "The House of the Dead 4 Special Test", "DVP-0010", "SBLS", THE_HOUSE_OF_THE_DEAD_4_SPECIAL_TEST,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1024, 768, GROUP_HOD4_SP_TEST,
         true, false, false, false, false, false, YELLOW},

        {0x13E59583, "The House of the Dead 4 Special Rev B", "DVP-0010B", "SBLS", THE_HOUSE_OF_THE_DEAD_4_SPECIAL_REVB,
         WORKING, SEGA_TYPE_3, SHOOTING, 1024, 768, GROUP_HOD4_SP,
         true, false, false, false, false, false, YELLOW},

        {0x302FEB00, "The House of the Dead 4 Special Rev B Test", "DVP-0010B", "SBLS", THE_HOUSE_OF_THE_DEAD_4_SPECIAL_REVB_TEST,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1024, 768, GROUP_HOD4_SP_TEST,
         true, false, false, false, false, false, YELLOW},

        {0x317F3B90, "The House of the Dead EX", "DVP-0063", "SBRC", THE_HOUSE_OF_THE_DEAD_EX,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1360, 768, GROUP_NONE,
         false, false, false, false, false, false, YELLOW},

        {0x3A5EEC69, "The House of the Dead EX Test", "DVP-0063", "SBRC", THE_HOUSE_OF_THE_DEAD_EX_TEST,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1360, 768, GROUP_NONE,
         false, false, false, false, false, false, YELLOW},

        // ========================================
        // Too Spicy
        // ========================================
        {0xFA0F6AB0, "Too Spicy", "DVP-0027A", "SBMV", TOO_SPICY,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_NONE,
         false, false, false, false, false, false, RED},

        {0x5A7F315E, "Too Spicy Test", "DVP-0027A", "SBMV", TOO_SPICY_TEST,
         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_NONE,
         false, false, false, false, false, false, RED},

        // ========================================
        // Virtua Fighter 5
        // ========================================
        {0xD409B70C, "Virtua Fighter 5", "DVP-0008", "SBLM", VIRTUA_FIGHTER_5,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        {0x08EBC0DB, "Virtua Fighter 5 Rev A", "DVP-0008A", "SBLM", VIRTUA_FIGHTER_5_REVA,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        {0xA47FBA2D, "Virtua Fighter 5 Rev B", "DVP-0008B", "SBLM", VIRTUA_FIGHTER_5_REVB,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        {0x8CA46167, "Virtua Fighter 5 Rev B", "DVP-0008C", "SBLM", VIRTUA_FIGHTER_5_REVC, 
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768,
         GROUP_VF5, false, false, false, false, false, false, YELLOW},

        {0x75946796, "Virtua Fighter 5 Rev E", "DVP-0008E", "SBLM", VIRTUA_FIGHTER_5_REVE,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        {0xB0A96E34, "Virtua Fighter 5 Export", "DVP-0043", "SBLM", VIRTUA_FIGHTER_5_EXPORT,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        {0xB95528F4, "Virtua Fighter 5 R", "DVP-0004", "SBQU", VIRTUA_FIGHTER_5_R,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        {0x012E4898, "Virtua Fighter 5 R Rev D", "DVP-0004D", "SBQU", VIRTUA_FIGHTER_5_R_REVD,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        {0x74465F9F, "Virtua Fighter 5 R Rev G", "DVP-0004G", "SBQU", VIRTUA_FIGHTER_5_R_REVG,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        {0xFCB9D941, "Virtua Fighter 5 Final Showdown Rev A", "DVP-5019A", "SBUV", VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_REVA,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        {0xAB70901C, "Virtua Fighter 5 Final Showdown Rev B", "DVP-5020", "SBXX", VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_REVB,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        {0x6BAA510D, "Virtua Fighter 5 Final Showdown Rev B  Ver 6000", "DVP-5020", "SBXX", VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_REVB_6000,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
         false, false, false, false, false, false, YELLOW},

        // ========================================
        // Virtua Tennis 3
        // ========================================
        {0x0E4BF4B1, "Virtua Tennis 3", "DVP-0005", "SBKX", VIRTUA_TENNIS_3,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3,
         false, false, false, true, false, false, YELLOW},

        {0x9E48AB5B, "Virtua Tennis 3 Test", "DVP-0005", "SBKX", VIRTUA_TENNIS_3_TEST,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3_TEST,
         false, false, false, true, false, false, YELLOW},

        {0xE4C64D01, "Virtua Tennis 3 Rev A", "DVP-0005A", "SBKX", VIRTUA_TENNIS_3_REVA,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3,
         false, false, false, true, false, false, YELLOW},

        {0x9C0E77E5, "Virtua Tennis 3 Rev A Test", "DVP-0005A", "SBKX", VIRTUA_TENNIS_3_REVA_TEST,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3_TEST,
         false, false, false, true, false, false, YELLOW},

        {0xA4BDB9E2, "Virtua Tennis 3 Rev B", "DVP-0005B", "SBKX", VIRTUA_TENNIS_3_REVB,
         WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3,
         false, false, false, true, false, false, YELLOW},

        {0x74E25472, "Virtua Tennis 3 Rev B Test", "DVP-0005B", "SBKX", VIRTUA_TENNIS_3_REVB_TEST,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3_TEST,
         false, false, false, true, false, false, YELLOW},

        {0x987AE3FF, "Virtua Tennis 3 Rev C", "DVP-0005C", "SBKX", VIRTUA_TENNIS_3_REVC,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3,
         false, false, false, true, false, false, YELLOW},

        {0x1E4271A4, "Virtua Tennis 3 Rev C Test", "DVP-0005C", "SBKX", VIRTUA_TENNIS_3_REVC_TEST,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3_TEST,
         false, false, false, true, false, false, YELLOW},

        {0x3CE2194F, "Skycurser", "", "", SKYCURSER,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_NONE,
         false, false, false, true, false, false, YELLOW},

        // ========================================
        // End marker
        // ========================================
        {0, nullptr, nullptr, nullptr, GAME_UNKNOWN,
         NOT_WORKING, SEGA_TYPE_3, DIGITAL, 0, 0, GROUP_NONE,
         false, false, false, false, false, false, YELLOW}
    };

    return database;
}

/**
 * @brief Find game entry by CRC32
 * @param crc32 CRC32 of the game executable
 * @return Pointer to game entry, or nullptr if not found
 */
inline const GameDataEntry* FindGameByCRC32(uint32_t crc32) {
    const GameDataEntry* db = GetGameDatabase();
    for (int i = 0; db[i].gameTitle != nullptr; i++) {
        if (db[i].crc32 == crc32) {
            return &db[i];
        }
    }
    return nullptr;
}

/**
 * @brief Find game entry by GameID enum
 * @param id GameID enum value
 * @return Pointer to game entry, or nullptr if not found
 */
inline const GameDataEntry* FindGameByID(GameID id) {
    const GameDataEntry* db = GetGameDatabase();
    for (int i = 0; db[i].gameTitle != nullptr; i++) {
        if (db[i].enumId == id) {
            return &db[i];
        }
    }
    return nullptr;
}
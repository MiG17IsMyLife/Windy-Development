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
        {0x2819B100, "After Burner Climax", "DVP-0009", "SBLR", AFTER_BURNER_CLIMAX,
         WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0xADC3F10A, "After Burner Climax Rev A", "DVP-0009A", "SBLR", AFTER_BURNER_CLIMAX_REVA,
         WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0xE7DD9BED, "After Burner Climax Rev B", "DVP-0009B", "SBLR", AFTER_BURNER_CLIMAX_REVB,
         WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0x9E2DE94B, "After Burner Climax SDX", "DVP-0018", "SBMN", AFTER_BURNER_CLIMAX_SDX,
         WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0x69A5CD6F, "After Burner Climax SDX Rev A", "DVP-0018A", "SBMN", AFTER_BURNER_CLIMAX_SDX_REVA,
         WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0x6E72F453, "After Burner Climax SE", "DVP-0040", "SBPB", AFTER_BURNER_CLIMAX_SE,
         WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        {0x8CE8FEE0, "After Burner Climax SE Rev A", "DVP-0040A", "SBPB", AFTER_BURNER_CLIMAX_SE_REVA,
         WORKING, SEGA_TYPE_1, GAME_TYPE_ABC, 640, 480, GROUP_ABC,
         false, false, false, false, false, false, YELLOW},

        // ========================================
        // Ghost Squad Evolution
        // ========================================
         {0x3F891F3E, "Ghost Squad Evolution", "DVP-0029A", "SBNJ", GHOST_SQUAD_EVOLUTION,
         WORKING, SEGA_TYPE_1, SHOOTING, 640, 480, GROUP_NONE,
         false, false, false, false, false, false, RED},

        // ========================================
        // Harley-Davidson
        // ========================================
        {0x5F9E65AD, "Harley-Davidson: King of the Road", "DVP-5007", "SBRG", HARLEY_DAVIDSON,
         WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_NONE,
         false, false, false, false, false, false, RED},

        // ========================================
        // Hummer
        // ========================================
        {0x1B8B5E1E, "Hummer", "DVP-0057B", "SBQN", HUMMER,
         WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_HUMMER,
         false, false, false, false, false, false, YELLOW},

        {0x17B21C5E, "Hummer SDLX", "DVP-0057", "SBQN", HUMMER_SDLX,
         WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_HUMMER,
         false, false, false, false, false, false, YELLOW},

        {0x0060E600, "Hummer Extreme", "DVP-0079", "SBST", HUMMER_EXTREME,
         WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_HUMMER,
         false, false, false, false, false, false, YELLOW},

        {0x05B53F39, "Hummer Extreme MDX", "DVP-0083", "SBST", HUMMER_EXTREME_MDX,
         WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_HUMMER,
         false, false, false, false, false, false, YELLOW},

         // ========================================
         // Initial D Arcade Stage 4
         // ========================================
         {0x56D9DD09, "Initial D Arcade Stage 4 Rev A", "DVP-0019A", "SBML", INITIALD_4_REVA,
          WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_JAP,
          false, false, false, false, true, false, YELLOW},

         {0x30ACC6DB, "Initial D Arcade Stage 4 Rev B", "DVP-0019B", "SBML", INITIALD_4_REVB,
          WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_JAP,
          false, false, false, false, true, false, YELLOW},

            {0x9C10D2DF, "Initial D Arcade Stage 4 Rev C", "DVP-0019C", "SBML", INITIALD_4_REVC,
             WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_JAP,
             false, false, false, false, true, false, YELLOW},

            {0x8A4E5B59, "Initial D Arcade Stage 4 Rev D", "DVP-0019D", "SBML", INITIALD_4_REVD,
             WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_JAP,
             false, false, false, false, true, false, YELLOW},

            {0x4A5BD894, "Initial D Arcade Stage 4 Rev G", "DVP-0019G", "SBML", INITIALD_4_REVG,
             WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_JAP,
             false, false, false, false, true, false, YELLOW},

            {0x4E6D41C1, "Initial D Arcade Stage 4 Export Rev B", "DVP-0030B", "SBNK", INITIALD_4_EXP_REVB,
             WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_EXP,
             false, false, false, false, true, false, YELLOW},

            {0x6C5ABF31, "Initial D Arcade Stage 4 Export Rev C", "DVP-0030C", "SBNK", INITIALD_4_EXP_REVC,
             WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_EXP,
             false, false, false, false, true, false, YELLOW},

            {0x5566C3B6, "Initial D Arcade Stage 4 Export Rev D", "DVP-0030D", "SBNK", INITIALD_4_EXP_REVD,
             WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID4_EXP,
             false, false, false, false, true, false, YELLOW},

             // ========================================
             // Initial D Arcade Stage 5
             // ========================================
             {0xFD7D9F4E, "Initial D Arcade Stage 5 Rev A", "DVP-0070A", "SBQZ", INITIALD_5_JAP_REVA,
              WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
              false, false, false, false, true, false, YELLOW},

             {0x10FCB6EB, "Initial D Arcade Stage 5 Rev C", "DVP-0070C", "SBQZ", INITIALD_5_JAP_REVC,
              WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
              false, false, false, false, true, false, YELLOW},

             {0x0D7CC5C9, "Initial D Arcade Stage 5 Rev F", "DVP-0070F", "SBQZ", INITIALD_5_JAP_REVF,
              WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
              false, false, false, false, true, false, YELLOW},

             {0xAEEE6BEF, "Initial D Arcade Stage 5 ServerBox", "DVP-0070", "SBQZ", INITIALD_5_JAP_SBQZ_SERVERBOX,
              WORKING, SEGA_TYPE_3, DRIVING, 640, 480, GROUP_ID_SERVERBOX   ,
              false, false, false, false, false, false, YELLOW},

             {0x74C85CC3, "Initial D5 EXP", "DVP-0075", "SBRY", INITIALD_5_EXP,
              WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
              false, false, false, false, true, false, YELLOW},

             {0xAB65E06E, "Initial D5 EXP 2.0", "DVP-0084", "SBTS", INITIALD_5_EXP_20,
              WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
              false, false, false, false, true, false, YELLOW},

             {0x8C9F0939, "Initial D5 EXP 2.0A", "DVP-0084A", "SBTS", INITIALD_5_EXP_20A,
              WORKING, SEGA_TYPE_3, DRIVING, 1360, 768, GROUP_ID5,
              false, false, false, false, true, false, YELLOW},

              // ========================================
              // Let's Go Jungle
              // ========================================
              {0xCC8FA878, "Let's Go Jungle!", "DVP-0011", "SBLU", LETS_GO_JUNGLE,
               WORKING, SEGA_TYPE_3, SHOOTING, 1360, 768, GROUP_LGJ,
               false, false, false, false, false, false, YELLOW},

              {0xA3A85C4E, "Let's Go Jungle! Rev A", "DVP-0011A", "SBLU", LETS_GO_JUNGLE_REVA,
               WORKING, SEGA_TYPE_3, SHOOTING, 1360, 768, GROUP_LGJ,
               false, false, false, false, false, false, YELLOW},

              {0x59E2EAF2, "Let's Go Jungle Special", "DVP-0036", "SBNU", LETS_GO_JUNGLE_SPECIAL,
               WORKING, SEGA_TYPE_3, SHOOTING, 1024, 768, GROUP_LGJ,
               true, false, false, false, false, false, YELLOW},

               // ========================================
               // Mahjong
               // ========================================
               {0x1F0C3B70, "Mj4 Rev G", "DVP-0049G", "SBPF", MJ4_REVG,
                NOT_WORKING, SEGA_TYPE_3, MAHJONG, 1360, 768, GROUP_NONE,
                false, false, false, false, false, true, YELLOW},

               {0x22B59D07, "Mj4 Evolution", "DVP-0081C", "SBRV", MJ4_EVO,
                NOT_WORKING, SEGA_TYPE_3, MAHJONG, 1360, 768, GROUP_NONE,
                false, false, false, false, false, true, YELLOW},

                // ========================================
                // OutRun 2 SP SDX
                // ========================================
                {0xC9570DE0, "Outrun 2 SP SDX", "DVP-0015", "SBMB", OUTRUN_2_SP_SDX,
                 WORKING, SEGA_TYPE_3, DRIVING, 800, 480, GROUP_OUTRUN,
                 false, true, true, false, false, false, YELLOW},

                {0xFE8C6E87, "Outrun 2 SP SDX Rev A", "DVP-0015A", "SBMB", OUTRUN_2_SP_SDX_REVA,
                 WORKING, SEGA_TYPE_3, DRIVING, 800, 480, GROUP_OUTRUN,
                 false, true, true, false, false, false, YELLOW},

                {0xF8B7F9CD, "Outrun 2 SP SDX Test", "DVP-0015", "SBMB", OUTRUN_2_SP_SDX_TEST,
                 NOT_WORKING, SEGA_TYPE_3, DRIVING, 800, 480, GROUP_OUTRUN_TEST,
                 false, true, true, false, false, false, YELLOW},

                {0x13AF8581, "Outrun 2 SP SDX Rev A Test", "DVP-0015A", "SBMB", OUTRUN_2_SP_SDX_REVA_TEST,
                 NOT_WORKING, SEGA_TYPE_3, DRIVING, 800, 480, GROUP_OUTRUN_TEST,
                 false, true, true, false, false, false, YELLOW},

                {0x37F6D3C9, "Outrun 2 SP SDX Rev A Test 2", "DVP-0015A", "SBMB", OUTRUN_2_SP_SDX_REVA_TEST2,
                 NOT_WORKING, SEGA_TYPE_3, DRIVING, 800, 480, GROUP_OUTRUN_TEST,
                 false, true, true, false, false, false, YELLOW},

                 // ========================================
                 // Primeval Hunt
                 // ========================================
                 {0x0C1CB630, "Primeval Hunt", "DVP-0048A", "SBPP", PRIMEVAL_HUNT,
                  WORKING, SEGA_TYPE_3, SHOOTING, 1360, 768, GROUP_NONE,
                  false, false, false, false, false, false, YELLOW},

                  // ========================================
                  // Quiz Answer x Answer
                  // ========================================
                  {0xD3A0A6A9, "Quiz Answer x Answer", "DVP-0069A", "SBQQ", QUIZ_AXA,
                   NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_NONE,
                   false, false, false, false, false, true, YELLOW},

                  {0xA7919F27, "Quiz Answer x Answer Live", "DVP-0078", "SBSA", QUIZ_AXA_LIVE,
                   NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_NONE,
                   false, false, false, false, false, true, YELLOW},

                   // ========================================
                   // Rambo
                   // ========================================
                   {0xD6CF12C1, "Rambo", "DVP-0034A", "SBNJ", RAMBO,
                    WORKING, SEGA_TYPE_3, SHOOTING, 640, 480, GROUP_RAMBO,
                    false, false, false, false, false, false, REDEX},

                   {0x30D986D4, "Rambo China", "DVP-0069A", "SBQQ", RAMBO_CHINA,
                    NOT_WORKING, SEGA_TYPE_3, SHOOTING, 640, 480, GROUP_RAMBO,
                    false, false, false, false, false, false, REDEX},

                    // ========================================
                    // R-Tuned
                    // ========================================
                    {0xEEE21E8C, "R-Tuned Ultimate Street Racing", "DVP-0060", "SBQW", R_TUNED,
                     WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_NONE,
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
                      {0xBBE45D86, "Sega Race TV", "DVP-0044", "SBPD", SEGA_RACE_TV,
                       WORKING, SEGA_TYPE_3, DRIVING, 1280, 768, GROUP_NONE,
                       false, false, false, false, false, false, YELLOW},

                       // ========================================
                       // The House of the Dead 4
                       // ========================================
                       {0xA90B2BEA, "The House of the Dead 4 Rev A", "DVP-0003A", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVA,
                        WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4,
                        false, false, false, false, false, false, YELLOW},

                       {0x1348BCA8, "The House of the Dead 4 Rev A Test", "DVP-0003A", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVA_TEST,
                        NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4_TEST,
                        false, false, false, false, false, false, YELLOW},

                       {0xB8B1E97E, "The House of the Dead 4 Rev B", "DVP-0003B", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVB,
                        WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4,
                        false, false, false, false, false, false, YELLOW},

                       {0x76FC6D3B, "The House of the Dead 4 Rev B Test", "DVP-0003B", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVB_TEST,
                        NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4_TEST,
                        false, false, false, false, false, false, YELLOW},

                       {0x6F16C801, "The House of the Dead 4 Rev C", "DVP-0003C", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVC,
                        WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4,
                        false, false, false, false, false, false, YELLOW},

                       {0x6E1CF723, "The House of the Dead 4 Rev C Test", "DVP-0003C", "SBLC", THE_HOUSE_OF_THE_DEAD_4_REVC_TEST,
                        NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_HOD4_TEST,
                        false, false, false, false, false, false, YELLOW},

                       {0x85E0ABA3, "The House of the Dead 4 Special", "DVP-0010", "SBLS", THE_HOUSE_OF_THE_DEAD_4_SPECIAL,
                        WORKING, SEGA_TYPE_3, SHOOTING, 1024, 768, GROUP_HOD4_SP,
                        true, false, false, false, false, false, YELLOW},

                       {0xE9DF8F4E, "The House of the Dead 4 Special Test", "DVP-0010", "SBLS", THE_HOUSE_OF_THE_DEAD_4_SPECIAL_TEST,
                        NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1024, 768, GROUP_HOD4_SP_TEST,
                        true, false, false, false, false, false, YELLOW},

                       {0x67766C33, "The House of the Dead 4 Special Rev B", "DVP-0010B", "SBLS", THE_HOUSE_OF_THE_DEAD_4_SPECIAL_REVB,
                        WORKING, SEGA_TYPE_3, SHOOTING, 1024, 768, GROUP_HOD4_SP,
                        true, false, false, false, false, false, YELLOW},

                       {0x4C04DA61, "The House of the Dead 4 Special Rev B Test", "DVP-0010B", "SBLS", THE_HOUSE_OF_THE_DEAD_4_SPECIAL_REVB_TEST,
                        NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1024, 768, GROUP_HOD4_SP_TEST,
                        true, false, false, false, false, false, YELLOW},

                       {0xFE271064, "The House of the Dead EX", "DVP-0063", "SBRC", THE_HOUSE_OF_THE_DEAD_EX,
                        WORKING, SEGA_TYPE_3, SHOOTING, 1360, 768, GROUP_NONE,
                        false, false, false, false, false, false, YELLOW},

                       {0x0D4864DD, "The House of the Dead EX Test", "DVP-0063", "SBRC", THE_HOUSE_OF_THE_DEAD_EX_TEST,
                        NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1360, 768, GROUP_NONE,
                        false, false, false, false, false, false, YELLOW},

                        // ========================================
                        // Too Spicy
                        // ========================================
                        {0x1FFC4E41, "Too Spicy", "DVP-0027A", "SBMV", TOO_SPICY,
                         WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_NONE,
                         false, false, false, false, false, false, RED},

                        {0x48A70893, "Too Spicy Test", "DVP-0027A", "SBMV", TOO_SPICY_TEST,
                         NOT_WORKING, SEGA_TYPE_3, SHOOTING, 1280, 768, GROUP_NONE,
                         false, false, false, false, false, false, RED},

                         // ========================================
                         // Virtua Fighter 5
                         // ========================================
                         {0xAA67B9F7, "Virtua Fighter 5", "DVP-0008", "SBLM", VIRTUA_FIGHTER_5,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                         {0x6AF7F893, "Virtua Fighter 5 Rev A", "DVP-0008A", "SBLM", VIRTUA_FIGHTER_5_REVA,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                         {0xBA1F798F, "Virtua Fighter 5 Rev B", "DVP-0008B", "SBLM", VIRTUA_FIGHTER_5_REVB,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                         {0xB78F9D9E, "Virtua Fighter 5 Rev E", "DVP-0008E", "SBLM", VIRTUA_FIGHTER_5_REVE,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                         {0x486DC71F, "Virtua Fighter 5 Export", "DVP-0043", "SBLM", VIRTUA_FIGHTER_5_EXPORT,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                         {0x37A5D6D0, "Virtua Fighter 5 R", "DVP-0058", "SBQU", VIRTUA_FIGHTER_5_R,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                         {0x0A10BABE, "Virtua Fighter 5 R Rev D", "DVP-0058D", "SBQU", VIRTUA_FIGHTER_5_R_REVD,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                         {0xECC8F8D8, "Virtua Fighter 5 R Rev G", "DVP-0058G", "SBQU", VIRTUA_FIGHTER_5_R_REVG,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                         {0xC403E356, "Virtua Fighter 5 Final Showdown Rev A", "DVP-5019A", "SBUV", VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_REVA,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                         {0x75FB0E2B, "Virtua Fighter 5 Final Showdown Rev B", "DVP-5020", "SBXX", VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_REVB,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                         {0xBBEE6C3D, "Virtua Fighter 5 Final Showdown Rev B 6000", "DVP-5020B", "SBXX", VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_REVB_6000,
                          WORKING, SEGA_TYPE_3, DIGITAL, 1280, 768, GROUP_VF5,
                          false, false, false, false, false, false, YELLOW},

                          // ========================================
                          // Virtua Tennis 3
                          // ========================================
                          {0x8D4E3C17, "Virtua Tennis 3", "DVP-0005", "SBKX", VIRTUA_TENNIS_3,
                           WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3,
                           false, false, false, true, false, false, YELLOW},

                          {0x21AB10A4, "Virtua Tennis 3 Test", "DVP-0005", "SBKX", VIRTUA_TENNIS_3_TEST,
                           NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3_TEST,
                           false, false, false, true, false, false, YELLOW},

                          {0xBD1B96A9, "Virtua Tennis 3 Rev A", "DVP-0005A", "SBKX", VIRTUA_TENNIS_3_REVA,
                           WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3,
                           false, false, false, true, false, false, YELLOW},

                          {0xA78B2C75, "Virtua Tennis 3 Rev A Test", "DVP-0005A", "SBKX", VIRTUA_TENNIS_3_REVA_TEST,
                           NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3_TEST,
                           false, false, false, true, false, false, YELLOW},

                          {0x5C254F42, "Virtua Tennis 3 Rev B", "DVP-0005B", "SBKX", VIRTUA_TENNIS_3_REVB,
                           WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3,
                           false, false, false, true, false, false, YELLOW},

                          {0xC27E7C57, "Virtua Tennis 3 Rev B Test", "DVP-0005B", "SBKX", VIRTUA_TENNIS_3_REVB_TEST,
                           NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3_TEST,
                           false, false, false, true, false, false, YELLOW},

                          {0xF30BF80B, "Virtua Tennis 3 Rev C", "DVP-0005C", "SBKX", VIRTUA_TENNIS_3_REVC,
                           WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3,
                           false, false, false, true, false, false, YELLOW},

                          {0xC2E03C4C, "Virtua Tennis 3 Rev C Test", "DVP-0005C", "SBKX", VIRTUA_TENNIS_3_REVC_TEST,
                           NOT_WORKING, SEGA_TYPE_3, DIGITAL, 1360, 768, GROUP_VT3_TEST,
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
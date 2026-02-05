#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include <stdint.h>

#include <windows.h>
#include <objbase.h>
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <vector>
#include <string>

// Core
#include "core/elfloader.h"
#include "core/symbolresolver.h"
#include "core/linuxstack.h"
#include "core/log.h"
#include "core/config.h"
#include "core/patches.h"
#include "core/gamedata.h"

// Hardware
#include "hardware/lindberghdevice.h"

// External functions from HardwareBridge
extern void InitHardwareBridge();
extern void CleanupHardwareBridge();

// Global variable that will hold the elf's crc32 as game id
uint8_t g_enumId = 0;
uint8_t g_grp = 0;

// =============================================================================
// Constants
// =============================================================================
static const size_t STACK_SIZE = 1024 * 1024;
static const char *DEFAULT_CONFIG_FILE = "windy.ini";

// =============================================================================
// CRC32 Calculation for Game Detection
// =============================================================================
static uint32_t g_crc32Table[256];
static bool g_crc32TableBuilt = false;

static void BuildCrc32Table()
{
    if (g_crc32TableBuilt)
        return;

    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
        g_crc32Table[i] = crc;
    }
    g_crc32TableBuilt = true;
}

static uint32_t CalculateFileCrc32(const char *filepath)
{
    BuildCrc32Table();

    std::ifstream file(filepath, std::ios::binary);
    if (!file)
    {
        log_error("CRC32: Cannot open file: %s", filepath);
        return 0;
    }

    uint32_t crc = 0xFFFFFFFF;
    char buffer[8192];

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
    {
        size_t bytesRead = (size_t)file.gcount();
        for (size_t i = 0; i < bytesRead; i++)
        {
            crc = g_crc32Table[(crc ^ (uint8_t)buffer[i]) & 0xFF] ^ (crc >> 8);
        }
    }

    return crc ^ 0xFFFFFFFF;
}

// =============================================================================
// Hardware Configuration based on Game
// =============================================================================
static void ConfigureHardwareForGame(const GameDataEntry *game, const WindyConfig &config)
{
    LindberghDevice &device = LindberghDevice::Instance();

    // -------------------------------------------------
    // JVS Board Configuration
    // -------------------------------------------------
    JvsBoard *jvs = device.GetJvsBoard();
    if (jvs)
    {
        // Set IOType based on game group
        // GROUP_ABC uses SEGA_TYPE_1, others use SEGA_TYPE_3
        if (game && game->gameGroup == GROUP_ABC)
        {
            jvs->SetIOType(SEGA_TYPE_1);
            log_info("JvsBoard: Set to SEGA_TYPE_1 (After Burner Climax mode)");
        }
        else
        {
            jvs->SetIOType(SEGA_TYPE_3);
            log_info("JvsBoard: Set to SEGA_TYPE_3 (Standard Lindbergh)");
        }
    }

    // -------------------------------------------------
    // EEPROM Board Configuration
    // -------------------------------------------------
    EepromBoard *eeprom = device.GetEepromBoard();
    if (eeprom)
    {
        // Apply config settings
        eeprom->SetRegion(static_cast<int>(config.region));
        eeprom->SetFreeplay(config.freeplay ? 1 : 0);

        if (config.emulateDriveboard)
        {
            log_info("EepromBoard: Driveboard emulation enabled");
        }
        if (config.emulateMotionboard)
        {
            log_info("EepromBoard: Motionboard emulation enabled");
        }

        // Game-specific patches
        if (game)
        {
            switch (game->gameGroup)
            {
                case GROUP_LGJ:
                case GROUP_HOD4:
                case GROUP_HOD4_SP:
                    // Let's Go Jungle / HOD4 Special: Fix credit section
                    eeprom->FixCreditSection();
                    log_info("EepromBoard: Applied credit section fix for %s", game->gameTitle);
                    break;

                case GROUP_HUMMER:
                    // Hummer series: Fix coin assignments
                    eeprom->FixCoinAssignmentsHummer();
                    log_info("EepromBoard: Applied Hummer coin assignment fix");
                    break;

                default:
                    break;
            }

            // Set game ID for any additional internal patches
            eeprom->EnableEmulationPatches(game->crc32);
        }

        // Network configuration for OutRun 2 SP SDX
        if (game && game->gameGroup == GROUP_OUTRUN)
        {
            if (config.networkIP[0] != '\0')
            {
                eeprom->SetNetworkIP(config.networkIP, config.networkNetmask);
                log_info("EepromBoard: Set network IP to %s for OutRun", config.networkIP);
            }
        }
    }

    // -------------------------------------------------
    // Security Board Configuration
    // -------------------------------------------------
    SecurityBoard *security = device.GetSecurityBoard();
    if (security)
    {
        security->SetDipResolution(config.width, config.height);
        log_info("SecurityBoard: DIP switches set for %dx%d", config.width, config.height);
    }

    // -------------------------------------------------
    // Base Board Configuration
    // -------------------------------------------------
    BaseBoard *baseboard = device.GetBaseBoard();
    if (baseboard)
    {
        // SRAM path can be configured per-game in future
        // baseboard->SetSramPath("sram.bin");
    }
}

// =============================================================================
// Helper: Change Working Directory to ELF Directory
// =============================================================================
static void ChangeDirectoryToElfLocation(const char *elfPath)
{
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];

    _splitpath(elfPath, drive, dir, fname, ext);

    char newDir[MAX_PATH];
    _makepath(newDir, drive, dir, NULL, NULL);

    if (SetCurrentDirectoryA(newDir))
    {
        log_info("Changed working directory to: %s", newDir);
    }
    else
    {
        log_warn("Failed to change working directory to: %s", newDir);
    }
}

// =============================================================================
// Print Banner
// =============================================================================
static void PrintBanner()
{
    log_info("==============================================");
    log_info("   Windy - SEGA Lindbergh Emulator");
    log_info("   Windows Native Port");
    log_info("   Build: %s %s", __DATE__, __TIME__);
    log_info("==============================================");
}

// =============================================================================
// Print Game Info
// =============================================================================
static void PrintGameInfo(const GameDataEntry *game, uint32_t crc32)
{
    if (game)
    {
        log_info("----------------------------------------------");
        log_info("  Game Detected!");
        log_info("  Name:   %s", game->gameTitle);
        log_info("  ID:     %s", game->gameID);
        log_info("  DVP:    %s", game->gameDVP);
        log_info("  CRC32:  0x%08X", crc32);
        log_info("  Group:  %d", game->gameGroup);
        log_info("----------------------------------------------");
    }
    else
    {
        log_warn("----------------------------------------------");
        log_warn("  Unknown Game!");
        log_warn("  CRC32: 0x%08X", crc32);
        log_warn("  Running with default settings...");
        log_warn("----------------------------------------------");
    }
}

// =============================================================================
// Print Configuration
// =============================================================================
static void PrintConfig(const WindyConfig &config)
{
    log_info("Configuration:");
    log_info("  Resolution:    %dx%d", config.width, config.height);
    log_info("  Fullscreen:    %s", config.fullscreen ? "Yes" : "No");
    log_info("  Region:        %d (%s)", config.region,
             config.region == REGION_JAPAN    ? "Japan"
             : config.region == REGION_US     ? "USA"
             : config.region == REGION_EXPORT ? "Export"
                                              : "Unknown");
    log_info("  Freeplay:      %s", config.freeplay ? "Yes" : "No");
    log_info("  Driveboard:    %s", config.emulateDriveboard ? "Enabled" : "Disabled");
    log_info("  Motionboard:   %s", config.emulateMotionboard ? "Enabled" : "Disabled");
    if (config.networkIP[0] != '\0')
    {
        log_info("  Network IP:    %s", config.networkIP);
    }
}

// (amDongle functions moved to src/core/patches.cpp)

// =============================================================================
// Main Entry Point
// =============================================================================
int main(int argc, char *argv[])
{
    // Initialize logging
    logInit();
    PrintBanner();

    // -------------------------------------------------
    // COM Initialization
    // -------------------------------------------------
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        log_warn("CoInitializeEx failed: 0x%08X", hr);
    }
    else
    {
        log_debug("COM initialized");
    }

    // -------------------------------------------------
    // Argument Parsing
    // -------------------------------------------------
    if (argc < 2)
    {
        log_error("Usage: Windy.exe <path_to_elf> [-c config.ini] [-t]");
        log_info("");
        log_info("Arguments:");
        log_info("  <path_to_elf>  - Path to the Lindbergh ELF executable");
        log_info("  -c <config>    - Optional path to configuration file (default: windy.ini)");
        log_info("  -t             - Enable Test Mode");
        system("pause");
        return 1;
    }

    const char *elfPath = argv[1];
    const char *configPath = DEFAULT_CONFIG_FILE;

    // Collect arguments for the guest
    std::vector<std::string> guestArgs;
    guestArgs.push_back(elfPath); // argv[0] is usually the program name

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc)
        {
            configPath = argv[i + 1];
            i++; // Skip next argument
        }
        else if (arg == "-t")
        {
            guestArgs.push_back(arg);
        }
        else
        {
            log_warn("Ignoring unknown argument: %s", arg.c_str());
        }
    }

    log_info("ELF Path:    %s", elfPath);
    log_info("Config Path: %s", configPath);

    // Log Guest Arguments
    log_info("Guest Arguments:");
    for (const auto &arg : guestArgs)
    {
        log_info("  %s", arg.c_str());
    }

    // -------------------------------------------------
    // Load Configuration
    // -------------------------------------------------
    log_info("Loading configuration...");
    ConfigManager &configMgr = ConfigManager::Instance();

    if (!configMgr.Load(configPath))
    {
        log_warn("Config file not found, using defaults.");
        // Create default config file for user reference
        configMgr.Save(configPath);
        log_info("Created default config: %s", configPath);
    }

    const WindyConfig &config = configMgr.GetConfig();
    PrintConfig(config);

    // -------------------------------------------------
    // Change Working Directory to ELF Location
    // -------------------------------------------------
    ChangeDirectoryToElfLocation(elfPath);

    // -------------------------------------------------
    // Calculate CRC32 and Detect Game
    // -------------------------------------------------
    log_info("Calculating CRC32 of ELF file...");
    uint32_t gameCrc32 = CalculateFileCrc32(elfPath);

    if (gameCrc32 == 0)
    {
        log_fatal("Failed to calculate CRC32. File may not exist.");
        system("pause");
        return 1;
    }

    log_info("CRC32: 0x%08X", gameCrc32);

    // Use FindGameByCRC32 from gamedata.h
    const GameDataEntry *detectedGame = FindGameByCRC32(gameCrc32);
    PrintGameInfo(detectedGame, gameCrc32);
    if (detectedGame)
    {
        g_enumId = detectedGame->enumId;
        g_grp = detectedGame->gameGroup;
    }

    // -------------------------------------------------
    // Initialize Hardware Bridge (VEH + Port I/O)
    // -------------------------------------------------
    log_info("Initializing Hardware Bridge...");
    InitHardwareBridge();

    // -------------------------------------------------
    // Initialize Lindbergh Device
    // -------------------------------------------------
    log_info("Initializing Lindbergh Hardware...");
    if (!LindberghDevice::Instance().Init())
    {
        log_fatal("Failed to initialize Lindbergh hardware!");
        CleanupHardwareBridge();
        system("pause");
        return 1;
    }

    // -------------------------------------------------
    // Configure Hardware for Detected Game
    // -------------------------------------------------
    log_info("Configuring hardware for game...");
    ConfigureHardwareForGame(detectedGame, config);

    // -------------------------------------------------
    // Load ELF File
    // -------------------------------------------------
    log_info("Loading ELF into memory...");
    ElfLoader loader(elfPath);

    if (!loader.LoadToMemory())
    {
        log_fatal("Failed to load ELF file!");
        CleanupHardwareBridge();
        system("pause");
        return 1;
    }

    log_info("ELF loaded successfully.");
    log_info("  Entry Point: 0x%08X", loader.GetHeader().e_entry);
    log_info("  JmpRel:      0x%08X", loader.GetJmpRel());
    log_info("  SymTab:      0x%08X", loader.GetSymTab());
    log_info("  StrTab:      0x%08X", loader.GetStrTab());
    log_info("  PltRelSize:  %u", loader.GetPltRelSize());

    // -------------------------------------------------
    // Resolve Symbols (GOT/PLT Patching)
    // -------------------------------------------------
    log_info("Resolving dynamic symbols...");
    SymbolResolver::ResolveAll(loader.GetJmpRel(), loader.GetSymTab(), loader.GetStrTab(), loader.GetPltRelSize());

    // -------------------------------------------------
    // Setup Linux Stack
    // -------------------------------------------------
    log_info("Setting up Linux stack...");

    // Use detected game name or default for argv[0] if no args provided?
    // Actually we now pass the full vector.
    // If the vector is empty (shouldn't be, has elfPath), we might want to ensure at least that.

    uint32_t finalEsp = LinuxStack::Setup(STACK_SIZE, guestArgs);
    uint32_t entryPoint = loader.GetHeader().e_entry;

    log_info("Stack ESP: 0x%08X", finalEsp);

    // -------------------------------------------------
    // Final Message
    // -------------------------------------------------
    log_info("==============================================");
    log_info("  All systems ready!");
    if (detectedGame)
    {
        log_info("  Starting: %s", detectedGame->gameTitle);
    }
    else
    {
        log_info("  Starting: Unknown Game (CRC: 0x%08X)", gameCrc32);
    }
    log_info("  Entry Point: 0x%08X", entryPoint);
    log_info("==============================================");

    // Small delay for log visibility
    Sleep(1000);
   
    // ---------------------------------------------------
    // Apply Patches (Game Specific)
    // ---------------------------------------------------
    log_info("Applying game specific patches...");
    Patches::Apply(g_enumId);

// FILE *f = fopen("dump.bin", "wb");
//     if (f)
//     {
//         fwrite((void*)0x8048000, 1, 316848, f);
//         fclose(f);
//     }

    // -------------------------------------------------
    // Jump to ELF Entry Point
    // -------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4731)
    __asm {
        mov eax, entryPoint
        mov esp, finalEsp

        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        xor esi, esi
        xor edi, edi

        xor ebp, ebp
        jmp eax
    }
#pragma warning(pop)
#else
    asm volatile("mov %0, %%esp\n\t"
                 "xor %%eax, %%eax\n\t"
                 "xor %%ebx, %%ebx\n\t"
                 "xor %%ecx, %%ecx\n\t"
                 "jmp *%1"
                 :
                 : "r"(finalEsp), "r"(entryPoint)
                 : "eax", "ebx", "ecx", "memory");
#endif

    // -------------------------------------------------
    // Cleanup (unreachable in normal flow)
    // -------------------------------------------------
    CleanupHardwareBridge();
    CoUninitialize();

    return 0;
}
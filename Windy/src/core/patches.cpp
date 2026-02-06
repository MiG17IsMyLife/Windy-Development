#include "patches.h"
#include "config.h"
#include "log.h"
#include "MinHook.h"
#include "../hardware/securityboard.h"
#include <cstdint>
#include <cstdio>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <cstdlib>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void *amDipswContextAddr;

// =============================================================
// amDongle Stubs
// =============================================================
static int amDongleInit()
{
    return 0;
}

static int amDongleIsAvailable()
{
    return 1;
}

static int amDongleUpdate()
{
    return 0;
}

static int amDongleIsDevelop()
{
    return 1;
}

static int amDongleUserInfoEx(int a, int b, char *_arcadeContext)
{
    // char *gameID;
    // switch (g_gameId)
    // {
    //     case RAMBO_SBQL:
    //         memcpy(_arcadeContext, "SBQL", 4);
    //         break;
    //     case HUMMER_EXTREME_SBST:
    //     case HUMMER_EXTREME_MDX_SBTL:
    //         memcpy(_arcadeContext, "SBST", 4);
    //         break;
    //     case GHOST_SQUAD_EVOLUTION_SBNJ:
    //         memcpy(_arcadeContext, getConfig()->gameID, 4);
    //         break;
    //     case INITIALD_4_SBML_REVA:
    //     case INITIALD_4_SBML_REVB:
    //     case INITIALD_4_SBML_REVC:
    //     case INITIALD_4_SBML_REVD:
    //     case INITIALD_4_SBML_REVG:
    //     case INITIALD_4_EXP_SBNK_REVB:
    //     case INITIALD_4_EXP_SBNK_REVC:
    //     case INITIALD_4_EXP_SBNK_REVD:
    //     case INITIALD_5_JAP_SBQZ_REVA:
    //     case INITIALD_5_JAP_SBQZ_REVC:
    //     case INITIALD_5_JAP_SBQZ_REVF:
    //     case INITIALD_5_EXP_SBRY:
    //     case INITIALD_5_EXP20_SBTS:
    //     case INITIALD_5_EXP20_SBTS_REVA:
    //         memcpy(_arcadeContext, elfID, 4);
    //         break;
    //     default:
    //         break;
    // }
    return 0;
}

// =============================================================
// Return Stubs
// =============================================================
static int stubRetZero()
{
    return 0;
}

static void stubReturn()
{
    return;
}

static int stubRetOne()
{
    return 1;
}

static int stubRetThree()
{
    return 3;
}

static int stubRetMinusOne()
{
    return -1;
}

static char stubRetZeroChar()
{
    return 0x00;
}

// =============================================================
// amDipsw Stubs
// =============================================================
int amDipswInit()
{
    uint32_t *amLibContext = (uint32_t *)amDipswContextAddr;
    *amLibContext = 1;
    return 0;
}

int amDipswExit()
{
    return 0;
}

int amDipswSetLed()
{
    return 0;
}

int amDipswGetData(uint8_t *dip)
{
    uint8_t result;
    uint32_t data;

    securityBoard.SecurityBoardIn(0x38, &data);

    result = 0x00;
    if ((~data & 0x10) != 0)
        result |= 0x01; // Dip 7
    if ((~data & 0x20) != 0)
        result |= 0x02; // Dip 8
    if ((~data & 0x40) != 0)
        result |= 0x04; // Dip 1
    if ((~data & 0x80) != 0)
        result |= 0x08; // Dip 2
    if ((~data & 0x100) != 0)
        result |= 0x10; // Rotation Dip 3
    if ((~data & 0x200) != 0)
        result |= 0x20; // Resolution Dip 4
    if ((~data & 0x400) != 0)
        result |= 0x40; // Resolution Dip 5
    if ((~data & 0x800) != 0)
        result |= 0x80; // Resolution Dip 6
    *dip = result;
    return 0;
}

// =============================================================
// Patches Implementation
// =============================================================

void Patches::PatchMemoryFromString(uintptr_t address, const char *value)
{
    std::string hex = value;
    if (hex.length() % 2 != 0)
    {
        log_error("Patch string len should be even.");
        return;
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(hex.length() / 2);

    for (size_t i = 0; i < hex.length(); i += 2)
    {
        std::string byteString = hex.substr(i, 2);
        bytes.push_back((uint8_t)strtol(byteString.c_str(), NULL, 16));
    }

    DWORD oldProtect;
    if (!VirtualProtect((void *)address, bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        log_error("Error: Cannot unprotect memory region at 0x%p", (void *)address);
        return;
    }

    memcpy((void *)address, bytes.data(), bytes.size());

    // Restore protection
    VirtualProtect((void *)address, bytes.size(), oldProtect, &oldProtect);
}

void Patches::Apply(uint8_t gameId)
{
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK)
    {
        log_error("Patches: MinHook initialization failed: %s", MH_StatusToString(status));
        return;
    }

    switch (gameId)
    {
        case OUTRUN_2_SP_SDX_REVA_TEST: // OutRun 2 SP SDX Rev A Test
        {
            log_info("Patches: Applying hooks for OutRun 2 SP SDX Rev A Test (0x%08X)", gameId);

            // Create hooks
            MH_CreateHook((void *)0x08066220, (void *)amDongleInit, NULL);
            MH_CreateHook((void *)0x080665a1, (void *)amDongleIsAvailable, NULL);
            MH_CreateHook((void *)0x080664c5, (void *)amDongleUpdate, NULL);
            MH_CreateHook((void *)0x080665c1, (void *)amDongleIsDevelop, NULL);

            amDipswContextAddr = (void *)0x080980e8; // Address of amDipswContext
            MH_CreateHook((void *)0x08066044, (void *)amDipswInit, NULL);
            MH_CreateHook((void *)0x080660e0, (void *)amDipswExit, NULL);
            MH_CreateHook((void *)0x08066156, (void *)amDipswGetData, NULL);
            MH_CreateHook((void *)0x080661ce, (void *)amDipswSetLed, NULL);
            break;
        }
        case INITIALD_5_JAP_SBQZ_SERVERBOX:
        {
            // Security
            MH_CreateHook((void *)0x080fd3ee, (void *)amDongleInit, NULL);
            MH_CreateHook((void *)0x080fbe39, (void *)amDongleIsAvailable, NULL);
            MH_CreateHook((void *)0x080fc89d, (void *)amDongleUpdate, NULL);

            amDipswContextAddr = (void *)0x083bc4e8; // Address of amDipswContext
            MH_CreateHook((void *)0x080fbbcc, (void *)amDipswInit, NULL);
            MH_CreateHook((void *)0x080fbc50, (void *)amDipswExit, NULL);
            MH_CreateHook((void *)0x080fbcc5, (void *)amDipswGetData, NULL);
            MH_CreateHook((void *)0x080fbd3c, (void *)amDipswSetLed, NULL); // amDipswSetLED
            break;
        }
        case THE_HOUSE_OF_THE_DEAD_4_REVA_TEST:
        {
            MH_CreateHook((void *)0x080677a0, (void *)amDongleInit, NULL);
            MH_CreateHook((void *)0x08067a81, (void *)amDongleIsAvailable, NULL);
            MH_CreateHook((void *)0x080679e8, (void *)amDongleUpdate, NULL);
            // Fixes
            amDipswContextAddr = (void *)0x080a7a48; // Address of amDipswContext
            MH_CreateHook((void *)0x08067548, (void *)amDipswInit, NULL);
            MH_CreateHook((void *)0x080675dd, (void *)amDipswExit, NULL);
            MH_CreateHook((void *)0x08067653, (void *)amDipswGetData, NULL);
            MH_CreateHook((void *)0x080676cb, (void *)amDipswSetLed, NULL);

            // CPU patch to support AMD processors
            PatchMemoryFromString(0x0804e46c, "9090909090"); //__intel_new_proc_init_P
            break;
        }
        case THE_HOUSE_OF_THE_DEAD_4_REVC_TEST:
        {
            MH_CreateHook((void *)0x0806228c, (void *)amDongleInit, NULL);
            MH_CreateHook((void *)0x0806259f, (void *)amDongleIsAvailable, NULL);
            MH_CreateHook((void *)0x08062506, (void *)amDongleUpdate, NULL);
            // //  Fixes
            amDipswContextAddr = (void *)0x080980e8; // Address of amDipswContext
            MH_CreateHook((void *)0x08062034, (void *)amDipswInit, NULL);
            MH_CreateHook((void *)0x080620c9, (void *)amDipswExit, NULL);
            MH_CreateHook((void *)0x0806213f, (void *)amDipswGetData, NULL);
            MH_CreateHook((void *)0x080621b7, (void *)amDipswSetLed, NULL);

            // CPU patch to support AMD processors
            PatchMemoryFromString(0x08049f4d, "9090909090"); //__intel_new_proc_init_P
            break;
        }
        case THE_HOUSE_OF_THE_DEAD_4_SPECIAL_REVB_TEST:
        {
            MH_CreateHook((void *)0x0806e914, (void *)amDongleInit, NULL);
            MH_CreateHook((void *)0x0806ec27, (void *)amDongleIsAvailable, NULL);
            MH_CreateHook((void *)0x0806eb8e, (void *)amDongleUpdate, NULL);
            // Fixes
            amDipswContextAddr = (void *)0x080adfe8; // Address of amDipswContext
            MH_CreateHook((void *)0x0806e6bc, (void *)amDipswInit, NULL);
            MH_CreateHook((void *)0x0806e751, (void *)amDipswExit, NULL);
            MH_CreateHook((void *)0x0806e7c7, (void *)amDipswGetData, NULL);
            MH_CreateHook((void *)0x0806e83f, (void *)amDipswSetLed, NULL);

            // CPU patch to support AMD processors
            PatchMemoryFromString(0x08054820, "9090909090"); //__intel_new_proc_init_P
            break;
        }
        case OUTRUN_2_SP_SDX_REVA:
        {
            // Security
            MH_CreateHook((void *)0x08190e80, (void *)amDongleInit, NULL);
            MH_CreateHook((void *)0x08191201, (void *)amDongleIsAvailable, NULL);
            MH_CreateHook((void *)0x08191125, (void *)amDongleUpdate, NULL);
            MH_CreateHook((void *)0x08191221, (void *)amDongleIsDevelop, NULL);
            // Fixes
            amDipswContextAddr = (void *)0x0896A088;
            MH_CreateHook((void *)0x08190ca4, (void *)amDipswInit, NULL);
            MH_CreateHook((void *)0x08190D40, (void *)amDipswExit, NULL);
            MH_CreateHook((void *)0x08190db6, (void *)amDipswGetData, NULL);
            MH_CreateHook((void *)0x08190e2e, (void *)amDipswSetLed, NULL);

            // Taken from original patched OUTRUN 2 SP SDX by Android and improved a little
            PatchMemoryFromString(0x08105317, "e91f000000");
            PatchMemoryFromString(0x08109593, "9090");
            PatchMemoryFromString(0x08109597, "9090");
            PatchMemoryFromString(0x0810959D, "77");

            // Bypass checks for Actuator and Force Feedback
            MH_CreateHook((void *)0x08103eaa, (void *)stubRetOne, NULL); // Steering wheel
            MH_CreateHook((void *)0x08105d88, (void *)stubRetOne, NULL); // Actuator
            break;
        }
        default:
            log_info("Patches: No patches defined for Game ID 0x%08X", gameId);
            break;
    }
    // Enable hooks
    status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK)
    {
        log_error("Patches: Failed to enable hooks: %s", MH_StatusToString(status));
    }
    else
    {
        log_info("Patches: Hooks enabled successfully");
    }
}

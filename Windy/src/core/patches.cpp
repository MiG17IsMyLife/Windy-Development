#define _CRT_SECURE_NO_WARNINGS

#include "patches.h"
#include "config.h"
#include "log.h"
#include "MinHook.h"
#include "../hardware/securityboard.h"
#include "../api/graphics/shaderpatches.h"
#include <cstdio>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <cstdlib>
#include <minwindef.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void *amDipswContextAddr;
static char elfID[4];

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
    GameID gameId = getConfig()->gameId;
    switch (gameId)
    {
        case INITIALD_4_REVD:
        case INITIALD_5_JAP_REVA:
            memcpy(_arcadeContext, elfID, 4);
            break;
        default:
            break;
    }
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

void _putConsole(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char output[1000];
    vsprintf(output, format, args);
    printf("%s", output);
    va_end(args);
    printf("\n");
}

// =============================================================
// Patches Implementation
// =============================================================

template <typename T> void detourFunction(size_t address, T function)
{
    MH_CreateHook((void *)address, (void *)function, NULL);
}

void replaceCallAtAddress(uintptr_t address, void *function)
{
    DWORD oldProtect;
    if (!VirtualProtect((void *)address, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        log_error("Error: Cannot unprotect memory region at 0x%p", (void *)address);
        return;
    }

    uint32_t callAddress = (uintptr_t)function - address - 5;

    char cave[5] = {(char)0xE8, 0x00, 0x00, 0x00, 0x00};
    cave[4] = (callAddress >> 24) & 0xFF;
    cave[3] = (callAddress >> 16) & 0xFF;
    cave[2] = (callAddress >> 8) & 0xFF;
    cave[1] = callAddress & 0xFF;

    memcpy((void *)address, cave, 5);

    VirtualProtect((void *)address, 5, oldProtect, &oldProtect);
}

void Patches::patchMemoryFromString(uintptr_t address, const char *value)
{
    size_t len = strlen(value);
    if (len % 2 != 0)
    {
        log_error("Patch string len should be even.");
        return;
    }

    DWORD oldProtect;
    if (!VirtualProtect((void *)address, len / 2, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        log_error("Error: Cannot unprotect memory region at 0x%p", (void *)address);
        return;
    }

    uint8_t *target = (uint8_t *)address;
    for (size_t i = 0; i < len; i += 2)
    {
        auto hexCharToByte = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return 0;
        };

        target[i / 2] = (hexCharToByte(value[i]) << 4) | hexCharToByte(value[i + 1]);
    }

    VirtualProtect((void *)address, len / 2, oldProtect, &oldProtect);
}

typedef char *(__stdcall *tTargetFunc2)(char *program, int e);

tTargetFunc2 foricGGetProgramStringAddress = nullptr;
char tmpShader[150000];

char *_cgGetProgramString(char *program, int e)
{
    printf("_cgGetProgramString Called\n");
    char *prgstr = foricGGetProgramStringAddress(program, e);

    if ((prgstr == NULL) || (*prgstr == '\0'))
    {
        return tmpShader;
    }
    else
    {
        if (sprintf(tmpShader, "%s", prgstr) < 0)
        {
            printf("Falied to save shader for next empty one.\n");
        }
    }
    return prgstr;
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
            detourFunction(0x08066220, amDongleInit);
            detourFunction(0x080665a1, amDongleIsAvailable);
            detourFunction(0x080664c5, amDongleUpdate);
            detourFunction(0x080665c1, amDongleIsDevelop);

            amDipswContextAddr = (void *)0x080980e8; // Address of amDipswContext
            detourFunction(0x08066044, amDipswInit);
            detourFunction(0x080660e0, amDipswExit);
            detourFunction(0x08066156, amDipswGetData);
            detourFunction(0x080661ce, amDipswSetLed);
            break;
        }
        case INITIALD_4_REVD:
        {
            // Security
            detourFunction(0x086e8276, amDongleInit);
            detourFunction(0x086e6cc1, amDongleIsAvailable);
            detourFunction(0x086e7725, amDongleUpdate);
            detourFunction(0x086e813d, amDongleUserInfoEx);
            memcpy(elfID, (void *)0x087929d8, 4); // Gets gameID from the ELF
            //  Fixes
            amDipswContextAddr = (void *)0x08da8a68; // Address of amDipswContext
            detourFunction(0x086e6a54, amDipswInit);
            detourFunction(0x086e6ad8, amDipswExit);
            detourFunction(0x086e6b4d, amDipswGetData);
            detourFunction(0x086e6bc4, amDipswSetLed); // amDipswSetLED

            detourFunction(0x0821f5cc, stubRetOne);          // isEthLinkUp
            patchMemoryFromString(0x082cd3b2, "c0270900");   // tickInitStoreNetwork
            patchMemoryFromString(0x082cd679, "e950010000"); // tickWaitDHCP
            patchMemoryFromString(0x082cedf8, "EB");         // Skip Kickback initialization
            patchMemoryFromString(0x087a024c, "f2");         // Skips initialization
            patchMemoryFromString(0x087a025c, "6f");         // Skips initialization
            // setVariable(0x0855f519, 0x000126e9);             // Avoid Full Screen set from Game

            patchMemoryFromString(0x0854ee03, "31C090"); // cgCreateProgram args argument to 0;
            detourFunction(0x08257470, stubRetOne);      // isExistNewerSource (forces shader recompilation)

            // uintptr_t origCgCreateProgram = *(uintptr_t *)0x08956eb8;
            // printf("Hooking _cgCreateProgram at 0x%p\n", (void *)origCgCreateProgram);
            // foricGCreateProgramAddress = (tTargetFunc)origCgCreateProgram;
            // replaceCallAtAddress(0x0854ee38, (void*)_cgCreateProgram);

            uintptr_t origCgGetProgramString = *(uintptr_t *)0x08957328;
            printf("Hooking _cgGetProgramString at 0x%p\n", (void *)origCgGetProgramString);
            foricGGetProgramStringAddress = (tTargetFunc2)origCgGetProgramString;
            replaceCallAtAddress(0x0854180c, (void*)_cgGetProgramString);
            replaceCallAtAddress(0x0854f99e, (void*)_cgGetProgramString);
            replaceCallAtAddress(0x0854fe3c, (void*)_cgGetProgramString);
            replaceCallAtAddress(0x0855211c, (void*)_cgGetProgramString);
            replaceCallAtAddress(0x085537ac, (void*)_cgGetProgramString);
            replaceCallAtAddress(0x08555b7a, (void*)_cgGetProgramString);
            break;
        }
        case INITIALD_5_JAP_SBQZ_SERVERBOX:
        {
            // Security
            detourFunction(0x080fd3ee, amDongleInit);
            detourFunction(0x080fbe39, amDongleIsAvailable);
            detourFunction(0x080fc89d, amDongleUpdate);

            amDipswContextAddr = (void *)0x083bc4e8; // Address of amDipswContext
            detourFunction(0x080fbbcc, amDipswInit);
            detourFunction(0x080fbc50, amDipswExit);
            detourFunction(0x080fbcc5, amDipswGetData);
            detourFunction(0x080fbd3c, amDipswSetLed); // amDipswSetLED
            break;
        }
        case OUTRUN_2_SP_SDX_REVA:
        {
            // Security
            detourFunction(0x08190e80, amDongleInit);
            detourFunction(0x08191201, amDongleIsAvailable);
            detourFunction(0x08191125, amDongleUpdate);
            detourFunction(0x08191221, amDongleIsDevelop);
            // Fixes
            amDipswContextAddr = (void *)0x0896A088;
            detourFunction(0x08190ca4, amDipswInit);
            detourFunction(0x08190D40, amDipswExit);
            detourFunction(0x08190db6, amDipswGetData);
            detourFunction(0x08190e2e, amDipswSetLed);

            // Taken from original patched OUTRUN 2 SP SDX by Android and improved a little
            patchMemoryFromString(0x08105317, "e91f000000");
            patchMemoryFromString(0x08109593, "9090");
            patchMemoryFromString(0x08109597, "9090");
            patchMemoryFromString(0x0810959D, "77");

            // Bypass checks for Actuator and Force Feedback
            detourFunction(0x08103eaa, stubRetOne); // Steering wheel
            detourFunction(0x08105d88, stubRetOne); // Actuator

            // detourFunction(0x0804ca98, ShaderPatches::my_glProgramStringARB);
            // Mesa Patches
            // detourFunction(0x0804cf38, ShaderPatches::glGenProgramsNV);             // Replaces glGenProgramsNV
            // detourFunction(0x0804c548, ShaderPatches::glBindProgramNV);             // Replaces glBindProgramNV
            // detourFunction(0x0804c788, ShaderPatches::glLoadProgramNV);             // Replaces glLoadProgramNV
            // detourFunction(0x0804c4e8, ShaderPatches::glDeleteProgramsNV);          // Replaces glDeleteProgramsNV
            // detourFunction(0x0804c938, ShaderPatches::glProgramParameter4fvNV);  // Replaces glProgramParameter4fvNV
            // detourFunction(0x0804ce18, ShaderPatches::glProgramParameters4fvNV); // Replaces glProgramParameters4fvNV
            // detourFunction(0x0804cec8, ShaderPatches::glProgramParameter4fNV);   // Replaces glProgramParameter4fNV
            // detourFunction(0x0804cb98, ShaderPatches::glIsProgramNV);               // Replaces glIsProgramNV
            // detourFunction(0x0804c568, ShaderPatches::glEndOcclusionQueryNV);       // Replaces glEndOcclusionQueryNV
            // detourFunction(0x0804d498, ShaderPatches::glGetOcclusionQueryuivNV);       // Replaces glGetOcclusionQueryuivNV
            // detourFunction(0x0804d648, ShaderPatches::glGenOcclusionQueriesNV);              // Replaces glGenOcclusionQueriesNV
            // detourFunction(0x0804d0a8, ShaderPatches::glBeginOcclusionQueryNV);     // Replaces glBeginOcclusionQueryNV
            // detourFunction(0x0804d158, ShaderPatches::glEnable);     // Patches glEnable
            // detourFunction(0x0804c888, ShaderPatches::glDisable);     // Patches glDisable


            break;
        }
        case INITIALD_5_JAP_REVA: // Initial D Arcade Stage 5 Japan Rev.A (DVP-0070A)
        {
            // Security
            detourFunction(0x089111da, amDongleInit);
            detourFunction(0x0890fc25, amDongleIsAvailable);
            detourFunction(0x08910689, amDongleUpdate);
            detourFunction(0x089110a1, amDongleUserInfoEx);
            memcpy(elfID, (void *)0x084e1707, 4); // Get gameID from the ELF

            // Fixes
            amDipswContextAddr = (void *)0x093d93a8;
            detourFunction(0x0890f9b8, amDipswInit);
            detourFunction(0x0890fa3c, amDipswExit);
            detourFunction(0x0890fab1, amDipswGetData);
            detourFunction(0x0890fb28, amDipswSetLed);

            detourFunction(0x08321968, stubRetOne);  // isEthLinkUp
            detourFunction(0x08307b62, stubRetOne);  // Skip Kickback initialization
            detourFunction(0x084de0dc, stubRetZero); // doesNeedRollerCleaning
            detourFunction(0x084de0f8, stubRetZero); // doesNeedStockerCleaning

            patchMemoryFromString(0x0843f068, "c0270900");     // tickInitStoreNetwork
            patchMemoryFromString(0x089e308c, "BA");           // Skips initialization
            patchMemoryFromString(0x08788e59, "e92601000090"); // Prevents Full Screen set from the game
            patchMemoryFromString(0x08441f99, "eb60");         // tickInitAddress

            // Shader
            detourFunction(0x08388cb4, stubRetOne); // isExistNewerSource (forces shader recompilation)
            // detourFunction(0x0807b370, gl_XGetProcAddressARB);
            patchMemoryFromString(0x0874433e, "00"); // Fix cutscenes

            // test
            detourFunction(0x08322092, stubRetZero);
            // patchMemoryFromString(0x0832226d, "9090909090");


            uintptr_t origCgGetProgramString = *(uintptr_t *)0x08c179e4;
            printf("Hooking _cgGetProgramString at 0x%p\n", (void *)origCgGetProgramString);
            foricGGetProgramStringAddress = (tTargetFunc2)origCgGetProgramString;
            replaceCallAtAddress(0x0875cbac, (void*)_cgGetProgramString);
            replaceCallAtAddress(0x0876a4ce, (void*)_cgGetProgramString);
            replaceCallAtAddress(0x0876a96c, (void*)_cgGetProgramString);
            replaceCallAtAddress(0x0876cc4c, (void*)_cgGetProgramString);
            replaceCallAtAddress(0x0876e2dc, (void*)_cgGetProgramString);
            replaceCallAtAddress(0x087706aa, (void*)_cgGetProgramString);
            break;
        }
        default:
            log_info("Patches: No patches defined for Game ID 0x%08X", gameId);
            break;
    }
    // Enable hooks
    // Skycurser test
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

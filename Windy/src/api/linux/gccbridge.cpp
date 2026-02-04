#include "gccbridge.h"
#include <windows.h>
#include <iostream>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// =============================================================
//   Library Management (MSYS2 / GCC Runtime)
// =============================================================
static HMODULE g_hMsys = NULL;
static HMODULE g_hLibGcc = NULL;

void EnsureMsysLoaded()
{
    if (!g_hMsys)
    {
        g_hMsys = LoadLibraryA("msys-2.0.dll");
        if (g_hMsys)
        {
            std::cout << "[Windy] MSYS2 runtime (msys-2.0.dll) loaded." << std::endl;
        }
        else
        {
            std::cerr << "[Windy] WARNING: Failed to load msys-2.0.dll." << std::endl;
        }
    }
}

void EnsureLibGccLoaded()
{
    if (!g_hLibGcc)
    {
        // loading MSYS2 GCC runtime
        g_hLibGcc = LoadLibraryA("msys-gcc_s-1.dll");

        if (g_hLibGcc)
        {
            std::cout << "[Windy] GCC Runtime loaded." << std::endl;
        }
        else
        {
            std::cerr << "[Windy] WARNING: FAILED to load GCC Runtime." << std::endl;
        }
    }
}

void *GetMsysSymbol(const char *name)
{
    EnsureMsysLoaded();
    if (g_hMsys)
        return (void *)GetProcAddress(g_hMsys, name);
    return NULL;
}

void *GetLibGccSymbol(const char *name)
{
    EnsureLibGccLoaded();
    if (g_hLibGcc)
        return (void *)GetProcAddress(g_hLibGcc, name);
    return NULL;
}

// =============================================================
//   Linux (glibc) Ctype Compatibility Implementation
// =============================================================

// Linux (glibc i386) ctype bitmasks
enum
{
    _ISupper = 0x100,
    _ISlower = 0x200,
    _ISalpha = 0x400,
    _ISdigit = 0x800,
    _ISxdigit = 0x1000,
    _ISspace = 0x2000,
    _ISprint = 0x4000,
    _ISgraph = 0x8000,
    _ISblank = 0x1,
    _IScntrl = 0x2,
    _ISpunct = 0x4,
    _ISalnum = 0x8
};

static unsigned short g_ctype_b[384]; // -128 to 255
static const unsigned short *g_ctype_b_ptr = g_ctype_b + 128;

static int32_t g_ctype_tolower[384];
static const int32_t *g_ctype_tolower_ptr = g_ctype_tolower + 128;

static int32_t g_ctype_toupper[384];
static const int32_t *g_ctype_toupper_ptr = g_ctype_toupper + 128;

static bool g_ctype_initialized = false;

static void InitLinuxCtype()
{
    if (g_ctype_initialized)
        return;

    // Initialize the ctype tables
    for (int i = -128; i < 256; i++)
    {
        int c = i;
        unsigned short flags = 0;

        if (i >= -1 && i <= 255)
        {
            // ASCII character range
            if (isupper(c))
                flags |= _ISupper | _ISalpha | _ISalnum | _ISprint | _ISgraph;
            if (islower(c))
                flags |= _ISlower | _ISalpha | _ISalnum | _ISprint | _ISgraph;
            if (isdigit(c))
                flags |= _ISdigit | _ISalnum | _ISprint | _ISgraph;
            if (isspace(c))
                flags |= _ISspace;
            if (isprint(c))
                flags |= _ISprint;
            if (isgraph(c))
                flags |= _ISgraph;
            if (iscntrl(c))
                flags |= _IScntrl;
            if (ispunct(c))
                flags |= _ISpunct | _ISprint | _ISgraph;
            if (isxdigit(c))
                flags |= _ISxdigit;
            if (c == ' ' || c == '\t')
                flags |= _ISblank;
        }

        g_ctype_b[128 + i] = flags;
        g_ctype_tolower[128 + i] = tolower(c);
        g_ctype_toupper[128 + i] = toupper(c);
    }
    g_ctype_initialized = true;
    std::cout << "[Windy] Initialized Linux Ctype table." << std::endl;
}

// =============================================================
//   Exported Functions (extern "C")
// =============================================================
extern "C"
{

    // --- Arithmetic Builtins ---
    int64_t __divdi3(int64_t a, int64_t b)
    {
        if (b == 0) return 0;
        return a / b;
    }
    uint64_t __udivdi3(uint64_t a, uint64_t b)
    {
        if (b == 0) {
            return 0;
        }
        return a / b;
    }
    uint64_t __umoddi3(uint64_t a, uint64_t b)
    {
        if (b == 0) {
            return 0;
        }
        return a % b;
    }
    int64_t __moddi3(int64_t a, int64_t b)
    {
        if (b == 0) return 0;
        return a % b;
    }
    uint64_t __fixunsdfdi(double a)
    {
        return (uint64_t)a;
    }
    uint64_t __fixunssfdi(float a)
    {
        return (uint64_t)a;
    }

    // --- Ctype Accessors ---
    const unsigned short **__ctype_b_loc(void)
    {
        InitLinuxCtype();
        return &g_ctype_b_ptr;
    }
    const int32_t **__ctype_tolower_loc(void)
    {
        InitLinuxCtype();
        return &g_ctype_tolower_ptr;
    }
    const int32_t **__ctype_toupper_loc(void)
    {
        InitLinuxCtype();
        return &g_ctype_toupper_ptr;
    }
    size_t __ctype_get_mb_cur_max()
    {
        return 1;
    }

    // --- GCC Internals ---

    int *__errno_location()
    {
        return _errno();
    }

    double __strtod_internal(const char *n, char **e, int g)
    {
        return strtod(n, e);
    }
    long __strtol_internal(const char *n, char **e, int b, int g)
    {
        return strtol(n, e, b);
    }
    unsigned long __strtoul_internal(const char *n, char **e, int b, int g)
    {
        return strtoul(n, e, b);
    }

    // C++ ABI stuffs but, do nothing
    int my_cxa_atexit(void (*func)(void *), void *arg, void *dso)
    {
        return 0;
    }
    void __libc_freeres()
    {
    }

    void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function)
    {
        fprintf(stderr, "ASSERTION FAILED: %s at %s:%d (%s)\n", assertion, file, line, function);
        TerminateProcess(GetCurrentProcess(), 1);
    }
}
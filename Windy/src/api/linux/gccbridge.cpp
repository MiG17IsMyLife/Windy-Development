#include "gccbridge.h"
#include <cstdint>
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

//#define arith64_u64 unsigned long long int 
//#define arith64_s64 signed long long int
//#define arith64_u32 unsigned int
//#define arith64_s32 int
//
//typedef union
//{
//    arith64_u64 u64;
//    arith64_s64 s64;
//    struct
//    {
//#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
//        arith64_u32 hi; arith64_u32 lo;
//#else
//        arith64_u32 lo; arith64_u32 hi;
//#endif
//    } u32;
//    struct
//    {
//#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
//        arith64_s32 hi; arith64_s32 lo;
//#else
//        arith64_s32 lo; arith64_s32 hi;
//#endif
//    } s32;
//} arith64_word;
//#define arith64_hi(n) (arith64_word){.u64=n}.u32.hi
//#define arith64_lo(n) (arith64_word){.u64=n}.u32.lo 
//
//// These functions return the number of leading 0-bits in a, starting at the
//// most significant bit position. If a is zero, the result is undefined.
//int __clzsi2(arith64_u32 a)
//{
//    int b, n = 0;
//    b = !(a & 0xffff0000) << 4; n += b; a <<= b;
//    b = !(a & 0xff000000) << 3; n += b; a <<= b;
//    b = !(a & 0xf0000000) << 2; n += b; a <<= b;
//    b = !(a & 0xc0000000) << 1; n += b; a <<= b;
//    return n + !(a & 0x80000000);
//}
//
//int __clzdi2(arith64_u64 a)
//{
//    int b, n = 0;
//    b = !(a & 0xffffffff00000000ULL) << 5; n += b; a <<= b;
//    b = !(a & 0xffff000000000000ULL) << 4; n += b; a <<= b;
//    b = !(a & 0xff00000000000000ULL) << 3; n += b; a <<= b;
//    b = !(a & 0xf000000000000000ULL) << 2; n += b; a <<= b;
//    b = !(a & 0xc000000000000000ULL) << 1; n += b; a <<= b;
//    return n + !(a & 0x8000000000000000ULL);
//}
//
//// These functions return the number of trailing 0-bits in a, starting at the
//// least significant bit position. If a is zero, the result is undefined.
//int __ctzsi2(arith64_u32 a)
//{
//    int b, n = 0;
//    b = !(a & 0x0000ffff) << 4; n += b; a >>= b;
//    b = !(a & 0x000000ff) << 3; n += b; a >>= b;
//    b = !(a & 0x0000000f) << 2; n += b; a >>= b;
//    b = !(a & 0x00000003) << 1; n += b; a >>= b;
//    return n + !(a & 0x00000001);
//}
//
//int __ctzdi2(arith64_u64 a)
//{
//    int b, n = 0;
//    b = !(a & 0x00000000ffffffffULL) << 5; n += b; a >>= b;
//    b = !(a & 0x000000000000ffffULL) << 4; n += b; a >>= b;
//    b = !(a & 0x00000000000000ffULL) << 3; n += b; a >>= b;
//    b = !(a & 0x000000000000000fULL) << 2; n += b; a >>= b;
//    b = !(a & 0x0000000000000003ULL) << 1; n += b; a >>= b;
//    return n + !(a & 0x0000000000000001ULL);
//} 
//
//// Calculate both the quotient and remainder of the unsigned division of a by
//// b. The return value is the quotient, and the remainder is placed in variable
//// pointed to by c (if it's not NULL).
//arith64_u64 __divmoddi4(arith64_u64 a, arith64_u64 b, arith64_u64 *c)
//{
//    if (b > a)                                  // divisor > numerator?
//    {
//        if (c) *c = a;                          // remainder = numerator
//        return 0;                               // quotient = 0
//    }
//    if (!arith64_hi(b))                         // divisor is 32-bit
//    {
//        if (b == 0)                             // divide by 0
//        {
//            volatile char x = 0; x = 1 / x;     // force an exception
//        }
//        if (b == 1)                             // divide by 1
//        {
//            if (c) *c = 0;                      // remainder = 0
//            return a;                           // quotient = numerator
//        }
//        if (!arith64_hi(a))                     // numerator is also 32-bit
//        {
//            if (c)                              // use generic 32-bit operators
//                *c = arith64_lo(a) % arith64_lo(b);
//            return arith64_lo(a) / arith64_lo(b);
//        }
//    }
//
//    // let's do long division
//    char bits = __clzdi2(b) - __clzdi2(a) + 1;  // number of bits to iterate (a and b are non-zero)
//    arith64_u64 rem = a >> bits;                // init remainder
//    a <<= 64 - bits;                            // shift numerator to the high bit
//    arith64_u64 wrap = 0;                       // start with wrap = 0
//    while (bits-- > 0)                          // for each bit
//    {
//        rem = (rem << 1) | (a >> 63);           // shift numerator MSB to remainder LSB
//        a = (a << 1) | (wrap & 1);              // shift out the numerator, shift in wrap
//        wrap = ((arith64_s64)(b - rem - 1) >> 63);  // wrap = (b > rem) ? 0 : 0xffffffffffffffff (via sign extension)
//        rem -= b & wrap;                        // if (wrap) rem -= b
//    }
//    if (c) *c = rem;                            // maybe set remainder
//    return (a << 1) | (wrap & 1);               // return the quotient
//}                     

// =============================================================
//   Exported Functions (extern "C")
// =============================================================
extern "C"
{

    // --- Arithmetic Builtins ---
    int64_t __divdi3(int64_t a, int64_t b)
    {
        return a / b;
    }
    uint64_t __udivdi3(uint64_t a, uint64_t b)
    {
        //return __divmoddi4(a, b, 0);
        return a / b;
    }
    uint64_t __umoddi3(uint64_t a, uint64_t b)
    {
        //uint64_t r;
        //__divmoddi4(a, b, &r);
        //return r;
        return a % b;
    }
    int64_t __moddi3(int64_t a, int64_t b)
    {
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
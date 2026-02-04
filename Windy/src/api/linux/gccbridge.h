#pragma once

#include <stdint.h>
#include <stddef.h>

// =============================================================
//   Helper Functions (for SymbolResolver)
// =============================================================
void EnsureMsysLoaded();
void EnsureLibGccLoaded();
void *GetMsysSymbol(const char *name);
void *GetLibGccSymbol(const char *name);

// =============================================================
//   Exported Functions (extern "C")
// =============================================================
extern "C"
{
    // --- Arithmetic Helpers ---
    int64_t __divdi3(int64_t a, int64_t b);
    uint64_t __udivdi3(uint64_t a, uint64_t b);
    uint64_t __umoddi3(uint64_t a, uint64_t b);
    int64_t __moddi3(int64_t a, int64_t b);
    uint64_t __fixunsdfdi(double a);
    uint64_t __fixunssfdi(float a);

    // --- Ctype Accessors ---
    const unsigned short **__ctype_b_loc(void);
    const int32_t **__ctype_tolower_loc(void);
    const int32_t **__ctype_toupper_loc(void);
    size_t __ctype_get_mb_cur_max(void);

    // --- GCC / Glibc Internals ---
    int *__errno_location();

    double __strtod_internal(const char *n, char **e, int g);
    long __strtol_internal(const char *n, char **e, int b, int g);
    unsigned long __strtoul_internal(const char *n, char **e, int b, int g);

    int my_cxa_atexit(void (*func)(void *), void *arg, void *dso);
    void __libc_freeres();
    void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function);
}
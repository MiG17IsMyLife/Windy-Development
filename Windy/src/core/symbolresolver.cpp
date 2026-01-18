#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define HAVE_STRUCT_TIMESPEC
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <objbase.h> 
#include <excpt.h>
#include <iostream>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <io.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <direct.h>
#include <process.h>
#include <map>
#include <algorithm>
#include <vector>
#include <GL/gl.h>

// --- Pthreads & Semaphore ---
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>

#include "SymbolResolver.h"
#include "common.h"
#include "../api/sega/libsegaapi.h"
#include "../api/graphics/X11Bridge.h"
#include "../api/graphics/GLXBridge.h"
#include "../api/libc/LibcBridge.h"
#include "../api/libc/PthreadBridge.h"
#include "../hardware/JvsBoard.h" 
#include "../hardware/BaseBoard.h" 
#include "../hardware/EepromBoard.h"

typedef unsigned short mode_t;
typedef unsigned int dev_t;

// --- Helper Macros ---
#define ELF32_R_SYM(val) ((val) >> 8)

// --- GPU Spoofing ---
static const char* GPU_VENDOR_STRING = "NVIDIA Corporation";
static const char* GPU_RENDERER_STRING = "GeForce 7800/PCIe/SSE2";
static const char* GPU_VERSION_STRING = "2.1.2 NVIDIA 285.05.09";
static const char* GPU_SL_VERSION_STRING = "1.20 NVIDIA via Cg compiler";

// --- Library Handles ---
static HMODULE g_hMsys = NULL;
static HMODULE g_hLibGcc = NULL;

static void EnsureMsysLoaded() {
    if (!g_hMsys) {
        g_hMsys = LoadLibraryA("msys-2.0.dll");
        if (g_hMsys) {
            std::cout << "[Windy] MSYS2 runtime (msys-2.0.dll) loaded." << std::endl;
        }
        else {
            std::cerr << "[Windy] WARNING: Failed to load msys-2.0.dll." << std::endl;
        }
    }
}

static void EnsureLibGccLoaded() {
    if (!g_hLibGcc) {
        // Loading msys stuffs
        g_hLibGcc = LoadLibraryA("msys-gcc_s-1.dll");
        if (!g_hLibGcc) g_hLibGcc = LoadLibraryA("libgcc_s_dw2-1.dll");

        if (g_hLibGcc) {
            std::cout << "[Windy] Loaded GCC Runtime (libgcc_s)" << std::endl;
        }
        else {
            std::cerr << "[Windy] WARNING: FAILED to load GCC Runtime. _Unwind_* symbols will fail." << std::endl;
            std::cerr << "  Please ensure 'msys-gcc_s-1.dll' is in the executable directory." << std::endl;
        }
    }
}

// =============================================================
//   Linux (glibc) Ctype Compatibility Definitions
// =============================================================
enum {
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
static const unsigned short* g_ctype_b_ptr = g_ctype_b + 128;
static int32_t g_ctype_tolower[384];
static const int32_t* g_ctype_tolower_ptr = g_ctype_tolower + 128;
static int32_t g_ctype_toupper[384];
static const int32_t* g_ctype_toupper_ptr = g_ctype_toupper + 128;
static bool g_ctype_initialized = false;

static void InitLinuxCtype() {
    if (g_ctype_initialized) return;

    // Initialize the ctype tables
    for (int i = -128; i < 256; i++) {
        int c = i;
        unsigned short flags = 0;

        // Flagging valid ASCII range
        if (i >= -1 && i <= 255) {
            if (isupper(c)) flags |= _ISupper | _ISalpha | _ISalnum | _ISprint | _ISgraph;
            if (islower(c)) flags |= _ISlower | _ISalpha | _ISalnum | _ISprint | _ISgraph;
            if (isdigit(c)) flags |= _ISdigit | _ISalnum | _ISprint | _ISgraph;
            if (isspace(c)) flags |= _ISspace;
            if (isprint(c)) flags |= _ISprint;
            if (isgraph(c)) flags |= _ISgraph;
            if (iscntrl(c)) flags |= _IScntrl;
            if (ispunct(c)) flags |= _ISpunct | _ISprint | _ISgraph;
            if (isxdigit(c)) flags |= _ISxdigit;
            if (c == ' ' || c == '\t') flags |= _ISblank;
        }

        g_ctype_b[128 + i] = flags;
        g_ctype_tolower[128 + i] = tolower(c);
        g_ctype_toupper[128 + i] = toupper(c);
    }
    g_ctype_initialized = true;
    std::cout << "[Windy] Initialized Linux Ctype table." << std::endl;
}

// --- GCC Builtins & Internals ---
extern "C" {
    // Arithmetic Helpers
    int64_t __divdi3(int64_t a, int64_t b) { return a / b; }
    uint64_t __udivdi3(uint64_t a, uint64_t b) { return a / b; }
    uint64_t __umoddi3(uint64_t a, uint64_t b) { return a % b; }
    int64_t __moddi3(int64_t a, int64_t b) { return a % b; }
    uint64_t __fixunsdfdi(double a) { return (uint64_t)a; }
    uint64_t __fixunssfdi(float a) { return (uint64_t)a; }

    int my_cxa_atexit(void (*func)(void*), void* arg, void* dso) { return 0; }
    int* __errno_location() { return _errno(); }
    void __libc_freeres() {}

    // Ctype functions (Using Linux-compatible table)
    const unsigned short** __ctype_b_loc(void) {
        InitLinuxCtype();
        return &g_ctype_b_ptr;
    }
    const int32_t** __ctype_tolower_loc(void) {
        InitLinuxCtype();
        return &g_ctype_tolower_ptr;
    }
    const int32_t** __ctype_toupper_loc(void) {
        InitLinuxCtype();
        return &g_ctype_toupper_ptr;
    }
    size_t __ctype_get_mb_cur_max() {
        return 1;
    }

    double __strtod_internal(const char* n, char** e, int g) { return strtod(n, e); }
    long __strtol_internal(const char* n, char** e, int b, int g) { return strtol(n, e, b); }
    unsigned long __strtoul_internal(const char* n, char** e, int b, int g) { return strtoul(n, e, b); }

    // Stub (Uses defined mode_t)
    int __xmknod(int ver, const char* path, mode_t mode, dev_t dev) { return -1; }
}

// --- Local Wrappers ---
extern "C" int my_usleep(unsigned int usec) { Sleep(usec / 1000); return 0; }
extern "C" unsigned int my_sleep(unsigned int seconds) { Sleep(seconds * 1000); return 0; }
extern "C" int my_gettimeofday(struct timeval* tv, void* tz) {
    if (tv) {
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        unsigned __int64 t = (unsigned __int64)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
        t -= 116444736000000000ULL; t /= 10;
        tv->tv_sec = (long)(t / 1000000); tv->tv_usec = (long)(t % 1000000);
    }
    return 0;
}
extern "C" struct tm* my_localtime_r(const time_t* timer, struct tm* buf) {
    struct tm* t = localtime(timer);
    if (t && buf) *buf = *t;
    return buf;
}

extern "C" void my_exit(int s) {
    std::cout << "[Program Exited] Code: " << s << std::endl;
    exit(s);
}
extern "C" int my_sprintf(char* s, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = vsprintf(s, fmt, args);
    va_end(args);
    return r;
}

// --- Hardware Device Management ---
static std::map<int, LindberghDevice*> g_openDevices;
static int g_nextFd = 1000;
static BaseBoard g_baseBoard;
static EepromBoard g_eepromBoard;
static JvsBoard g_jvsBoard;

// --- I/O Wrappers (Hardware Aware) ---
extern "C" int my_open(const char* pathname, int flags, ...) {
    std::cout << "[IO] open: " << pathname << std::endl;
    if (strncmp(pathname, "/proc/", 6) == 0) return -1;

    LindberghDevice* target = nullptr;
    if (strstr(pathname, "/dev/lbbb")) target = &g_baseBoard;
    else if (strstr(pathname, "/dev/i2c")) target = &g_eepromBoard;
    else if (strstr(pathname, "/dev/ttyS")) target = &g_jvsBoard;
    else if (strncmp(pathname, "/dev/", 5) == 0) return _open("NUL", _O_RDWR);

    if (target && target->Open()) {
        int fd = g_nextFd++;
        g_openDevices[fd] = target;
        return fd;
    }

    int win_flags = _O_BINARY;
    if ((flags & 3) == 0) win_flags |= _O_RDONLY;
    if ((flags & 3) == 1) win_flags |= _O_WRONLY;
    if ((flags & 3) == 2) win_flags |= _O_RDWR;
    if (flags & 0x40) win_flags |= _O_CREAT;

    return _open(pathname, win_flags, _S_IREAD | _S_IWRITE);
}

extern "C" int my_close(int fd) {
    if (g_openDevices.count(fd)) { g_openDevices.erase(fd); return 0; }
    return _close(fd);
}
extern "C" int my_read(int fd, void* buf, size_t count) {
    if (g_openDevices.count(fd)) return g_openDevices[fd]->Read(buf, count);
    return _read(fd, buf, (unsigned int)count);
}
extern "C" int my_write(int fd, const void* buf, size_t count) {
    if (g_openDevices.count(fd)) return g_openDevices[fd]->Write(buf, count);
    if (fd == 1 || fd == 2) { fwrite(buf, 1, count, stdout); return count; }
    return _write(fd, buf, (unsigned int)count);
}
extern "C" int my_ioctl(int fd, unsigned long request, ...) {
    va_list args; va_start(args, request); void* argp = va_arg(args, void*); va_end(args);
    if (g_openDevices.count(fd)) return g_openDevices[fd]->Ioctl(request, argp);
    return 0;
}

extern "C" const GLubyte* my_glGetString(GLenum name) {
    switch (name) {
    case 0x1F00: return (const GLubyte*)GPU_VENDOR_STRING;
    case 0x1F01: return (const GLubyte*)GPU_RENDERER_STRING;
    case 0x1F02: return (const GLubyte*)GPU_VERSION_STRING;
    case 0x8B8C: return (const GLubyte*)GPU_SL_VERSION_STRING;
    }
    static auto real_glGetString = (const GLubyte * (APIENTRY*)(GLenum))GetProcAddress(LoadLibraryA("opengl32.dll"), "glGetString");
    if (real_glGetString) return real_glGetString(name);
    return NULL;
}

// --- Memory Wrappers (Alignment) ---
extern "C" void* my_malloc(size_t s) { return _aligned_malloc(s, 16); }
extern "C" void my_free(void* p) { if (p) _aligned_free(p); }
extern "C" void* my_calloc(size_t n, size_t s) {
    size_t total = n * s;
    void* p = _aligned_malloc(total, 16);
    if (p) memset(p, 0, total);
    return p;
}
extern "C" void* my_realloc(void* p, size_t s) { return _aligned_realloc(p, s, 16); }
extern "C" void* my_memalign(size_t a, size_t s) { return _aligned_malloc(s, a); }

extern "C" void __assert_fail(const char* a, const char* f, unsigned int l, const char*) {
    printf("ASSERT: %s at %s:%d\n", a, f, l); TerminateProcess(GetCurrentProcess(), 1);
}

void __declspec(naked) UnimplementedStub() {
    __asm { pushad }
    MessageBoxA(NULL, "Unimplemented Function Called", "Error", MB_OK);
    TerminateProcess(GetCurrentProcess(), 1);
}

extern "C" int my_stub_success() { return 0; }
extern "C" int my_stub_fail() { return -1; }
extern "C" void* my_stub_null() { return NULL; }

int ExceptionFilter(unsigned int code, struct _EXCEPTION_POINTERS* ep) {
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR addr = ep->ExceptionRecord->ExceptionInformation[1];
        void* page = (void*)(addr & ~(0xFFF));
        if (VirtualAlloc(page, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)) {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

typedef int (*MainFunc)(int, char**, char**);
extern "C" void my_libc_start_main(MainFunc m, int c, char** a, void (*i)(), void (*f)(), void (*r)(), void* s) {
    std::cout << "[Translator] Starting via __libc_start_main..." << std::endl;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (i) {
        std::cout << "[Translator] Running init..." << std::endl;
        __try { i(); }
        __except (ExceptionFilter(GetExceptionCode(), GetExceptionInformation())) {}
    }
    std::cout << "[Translator] Calling main()..." << std::endl;
    int result = 0;
    __try { result = m(c, a, nullptr); }
    __except (ExceptionFilter(GetExceptionCode(), GetExceptionInformation())) {
        std::cerr << "[Translator] Process terminated during main." << std::endl; exit(1);
    }
    exit(result);
}

// =============================================================
//   Symbol Resolver Main Function
// =============================================================
uintptr_t SymbolResolver::GetExternalAddr(const char* name) {
    if (strcmp(name, "glGetString") == 0) return (uintptr_t)&my_glGetString;

    // --- NVIDIA Cg Toolkit ---
    if (strncmp(name, "cg", 2) == 0) {
        static HMODULE hCg = NULL;
        static HMODULE hCgGL = NULL;
        static bool attemptedLoad = false;
        if (!attemptedLoad) {
            hCg = LoadLibraryA("cg.dll");
            if (hCg) std::cout << "[Windy] Loaded cg.dll" << std::endl;
            hCgGL = LoadLibraryA("cgGL.dll");
            if (hCgGL) std::cout << "[Windy] Loaded cgGL.dll" << std::endl;
            attemptedLoad = true;
        }
        void* proc = NULL;
        if (hCg) proc = (void*)GetProcAddress(hCg, name);
        if (!proc && hCgGL) proc = (void*)GetProcAddress(hCgGL, name);
        if (proc) return (uintptr_t)proc;
    }

#define MAP(n, f) if (strcmp(name, n) == 0) return (uintptr_t)&f

    // ============================================================
    // GCC / GLIBC INTERNALS
    // ============================================================
    MAP("__ctype_b_loc", __ctype_b_loc);
    MAP("__ctype_tolower_loc", __ctype_tolower_loc);
    MAP("__ctype_toupper_loc", __ctype_toupper_loc);
    MAP("__ctype_get_mb_cur_max", __ctype_get_mb_cur_max);

    MAP("__divdi3", __divdi3);
    MAP("__udivdi3", __udivdi3);
    MAP("__umoddi3", __umoddi3);
    MAP("__moddi3", __moddi3);
    MAP("__fixunsdfdi", __fixunsdfdi);
    MAP("__fixunssfdi", __fixunssfdi);

    MAP("__strtod_internal", __strtod_internal);
    MAP("__strtol_internal", __strtol_internal);
    MAP("__strtoul_internal", __strtoul_internal);
    MAP("__libc_start_main", my_libc_start_main);
    MAP("__cxa_atexit", my_cxa_atexit);
    MAP("__assert_fail", __assert_fail);
    MAP("__errno_location", __errno_location);
    MAP("__libc_freeres", __libc_freeres);
    MAP("__xmknod", __xmknod);

    // ============================================================
    // FILE I/O
    // ============================================================
    MAP("fopen", LibcBridge::fopen_wrapper);
    MAP("fopen64", LibcBridge::fopen64_wrapper);
    MAP("fclose", LibcBridge::fclose_wrapper);
    MAP("fread", LibcBridge::fread_wrapper);
    MAP("fwrite", LibcBridge::fwrite_wrapper);
    MAP("fseek", LibcBridge::fseek_wrapper);
    MAP("fseeko64", LibcBridge::fseeko64_wrapper);
    MAP("lseek64", _lseeki64);
    MAP("ftell", LibcBridge::ftell_wrapper);
    MAP("ftello64", LibcBridge::ftello64_wrapper);
    MAP("rewind", LibcBridge::rewind_wrapper);
    MAP("fgetc", LibcBridge::fgetc_wrapper);
    MAP("getc", LibcBridge::getc_wrapper);
    MAP("fgets", LibcBridge::fgets_wrapper);
    MAP("ungetc", LibcBridge::ungetc_wrapper);
    MAP("fputc", LibcBridge::fputc_wrapper);
    MAP("putc", LibcBridge::putc_wrapper);
    MAP("fputs", LibcBridge::fputs_wrapper);
    MAP("puts", LibcBridge::puts_wrapper);
    MAP("putchar", LibcBridge::putchar_wrapper);
    MAP("fprintf", LibcBridge::fprintf_wrapper);
    MAP("vfprintf", LibcBridge::vfprintf_wrapper);
    MAP("printf", LibcBridge::printf_wrapper);
    MAP("vprintf", LibcBridge::vprintf_wrapper);
    MAP("creat", LibcBridge::creat_wrapper);
    MAP("fscanf", LibcBridge::fscanf_wrapper);
    MAP("fflush", LibcBridge::fflush_wrapper);
    MAP("setvbuf", LibcBridge::setvbuf_wrapper);
    MAP("fileno", LibcBridge::fileno_wrapper);
    MAP("ferror", LibcBridge::ferror_wrapper);
    MAP("feof", LibcBridge::feof_wrapper);
    MAP("perror", LibcBridge::perror_wrapper);
    MAP("fdopen", LibcBridge::fdopen_wrapper);
    MAP("popen", _popen);
    MAP("pclose", _pclose);

    // ============================================================
    // FORMATTED I/O
    // ============================================================
    MAP("sprintf", my_sprintf);
    MAP("snprintf", LibcBridge::snprintf_wrapper);
    MAP("vsprintf", LibcBridge::vsprintf_wrapper);
    MAP("vsnprintf", LibcBridge::vsnprintf_wrapper);
    MAP("sscanf", LibcBridge::sscanf_wrapper);

    // ============================================================
    // STRING FUNCTIONS
    // ============================================================
    MAP("strchr", LibcBridge::strchr_wrapper);
    MAP("strrchr", LibcBridge::strrchr_wrapper);
    MAP("strstr", LibcBridge::strstr_wrapper);
    MAP("strtok_r", LibcBridge::strtok_r_wrapper);
    MAP("strncat", LibcBridge::strncat_wrapper);
    MAP("strncpy", LibcBridge::strncpy_wrapper);
    MAP("strerror", LibcBridge::strerror_wrapper);
    MAP("strdup", LibcBridge::strdup_wrapper);
    MAP("strcasecmp", LibcBridge::strcasecmp_wrapper);
    MAP("strncasecmp", _strnicmp);
    MAP("index", LibcBridge::strchr_wrapper);
    MAP("rindex", LibcBridge::strrchr_wrapper);
    MAP("strcoll", LibcBridge::strcoll_wrapper);
    MAP("strxfrm", LibcBridge::strxfrm_wrapper);
    MAP("strcpy", strcpy);
    MAP("strlen", strlen);
    MAP("strcmp", strcmp);
    MAP("strncmp", strncmp);
    MAP("strcat", strcat);

    // ============================================================
    // MEMORY FUNCTIONS
    // ============================================================
    MAP("memchr", LibcBridge::memchr_wrapper);
    MAP("memmove", LibcBridge::memmove_wrapper);
    MAP("malloc", my_malloc);
    MAP("free", my_free);
    MAP("calloc", my_calloc);
    MAP("realloc", my_realloc);
    MAP("memalign", my_memalign);
    MAP("memcpy", memcpy);
    MAP("memset", memset);

    // ============================================================
    // PATH / FILESYSTEM
    // ============================================================
    MAP("getcwd", LibcBridge::getcwd_wrapper);
    MAP("realpath", LibcBridge::realpath_wrapper);
    MAP("remove", LibcBridge::remove_wrapper);
    MAP("mkdir", _mkdir);
    MAP("rmdir", _rmdir);
    MAP("unlink", _unlink);
    MAP("access", _access);

    // ============================================================
    // TIME / DATE
    // ============================================================
    MAP("time", time);
    MAP("utime", LibcBridge::utime_wrapper);
    MAP("gettimeofday", my_gettimeofday);
    MAP("usleep", my_usleep);
    MAP("sleep", my_sleep);
    MAP("nanosleep", LibcBridge::nanosleep_wrapper);
    MAP("localtime", LibcBridge::localtime_wrapper);
    MAP("localtime_r", my_localtime_r);
    MAP("mktime", LibcBridge::mktime_wrapper);
    MAP("strftime", LibcBridge::strftime_wrapper);
    MAP("settimeofday", LibcBridge::settimeofday_wrapper);

    // ============================================================
    // LOCALE / WIDE CHAR
    // ============================================================
    MAP("setlocale", LibcBridge::setlocale_wrapper);
    MAP("btowc", LibcBridge::btowc_wrapper);
    MAP("wctob", LibcBridge::wctob_wrapper);
    MAP("mbrtowc", LibcBridge::mbrtowc_wrapper);
    MAP("wcrtomb", LibcBridge::wcrtomb_wrapper);
    MAP("getwc", LibcBridge::getwc_wrapper);
    MAP("putwc", LibcBridge::putwc_wrapper);
    MAP("ungetwc", LibcBridge::ungetwc_wrapper);
    MAP("wcslen", LibcBridge::wcslen_wrapper);
    MAP("wmemcpy", LibcBridge::wmemcpy_wrapper);
    MAP("wmemmove", LibcBridge::wmemmove_wrapper);
    MAP("wmemset", LibcBridge::wmemset_wrapper);
    MAP("wmemchr", LibcBridge::wmemchr_wrapper);
    MAP("wmemcmp", LibcBridge::wmemcmp_wrapper);
    MAP("wcscoll", LibcBridge::wcscoll_wrapper);
    MAP("wcsxfrm", LibcBridge::wcsxfrm_wrapper);
    MAP("wcsftime", LibcBridge::wcsftime_wrapper);

    MAP("tolower", LibcBridge::tolower_wrapper);
    MAP("toupper", LibcBridge::toupper_wrapper);
    MAP("towlower", LibcBridge::towlower_wrapper);
    MAP("towupper", LibcBridge::towupper_wrapper);
    MAP("wctype", LibcBridge::wctype_wrapper);
    MAP("iswctype", LibcBridge::iswctype_wrapper);

    // ============================================================
    // STDLIB / SYSTEM
    // ============================================================
    MAP("atoi", atoi);
    MAP("rand", LibcBridge::rand_wrapper);
    MAP("srand", LibcBridge::srand_wrapper);
    MAP("getenv", LibcBridge::getenv_wrapper);
    MAP("system", LibcBridge::system_wrapper);
    MAP("abort", LibcBridge::abort_wrapper);
    MAP("raise", LibcBridge::raise_wrapper);
    MAP("exit", my_exit);
    MAP("_exit", LibcBridge::_exit_wrapper);

    // ============================================================
    // PTHREADS
    // ============================================================
    MAP("pthread_create", PthreadBridge::create_wrapper);
    MAP("pthread_join", PthreadBridge::join_wrapper);
    MAP("pthread_detach", PthreadBridge::detach_wrapper);
    MAP("pthread_exit", pthread_exit);
    MAP("pthread_self", pthread_self);
    MAP("pthread_equal", pthread_equal);
    MAP("pthread_cancel", pthread_cancel);
    MAP("pthread_once", pthread_once);

    MAP("pthread_attr_init", pthread_attr_init);
    MAP("pthread_attr_destroy", pthread_attr_destroy);
    MAP("pthread_attr_setstacksize", pthread_attr_setstacksize);
    MAP("pthread_attr_getstacksize", pthread_attr_getstacksize);
    MAP("pthread_attr_setdetachstate", pthread_attr_setdetachstate);
    MAP("pthread_attr_setschedparam", pthread_attr_setschedparam);
    MAP("pthread_attr_setschedpolicy", pthread_attr_setschedpolicy);

    MAP("pthread_setschedparam", pthread_setschedparam);
    MAP("pthread_getschedparam", pthread_getschedparam);
    MAP("sched_yield", sched_yield);
    MAP("sched_get_priority_min", sched_get_priority_min);
    MAP("sched_get_priority_max", sched_get_priority_max);
    MAP("sched_getaffinity", sched_getaffinity);
    MAP("sched_setaffinity", sched_setaffinity);

    MAP("pthread_mutex_init", PthreadBridge::mutex_init_wrapper);
    MAP("pthread_mutex_destroy", PthreadBridge::mutex_destroy_wrapper);
    MAP("pthread_mutex_lock", PthreadBridge::mutex_lock_wrapper);
    MAP("pthread_mutex_trylock", PthreadBridge::mutex_trylock_wrapper);
    MAP("pthread_mutex_unlock", PthreadBridge::mutex_unlock_wrapper);
    MAP("pthread_mutexattr_init", pthread_mutexattr_init);
    MAP("pthread_mutexattr_destroy", pthread_mutexattr_destroy);
    MAP("pthread_mutexattr_settype", pthread_mutexattr_settype);

    // ============================================================
    // PTHREADS - CONDITION VARIABLES
    // ============================================================
    MAP("pthread_cond_init", PthreadBridge::cond_init_wrapper);
    MAP("pthread_cond_destroy", PthreadBridge::cond_destroy_wrapper);
    MAP("pthread_cond_wait", PthreadBridge::cond_wait_wrapper);
    MAP("pthread_cond_timedwait", pthread_cond_timedwait);
    MAP("pthread_cond_signal", PthreadBridge::cond_signal_wrapper);
    MAP("pthread_cond_broadcast", PthreadBridge::cond_broadcast_wrapper);

    // ============================================================
    // PTHREADS - THREAD-LOCAL STORAGE
    // ============================================================
    MAP("pthread_key_create", pthread_key_create);
    MAP("pthread_key_delete", pthread_key_delete);
    MAP("pthread_setspecific", pthread_setspecific);
    MAP("pthread_getspecific", pthread_getspecific);

    // ============================================================
    // SEMAPHORES
    // ============================================================
    MAP("sem_init", sem_init);
    MAP("sem_destroy", sem_destroy);
    MAP("sem_wait", sem_wait);
    MAP("sem_trywait", sem_trywait);
    MAP("sem_post", sem_post);
    MAP("sem_getvalue", sem_getvalue);

    // ============================================================
    // LOW-LEVEL I/O
    // ============================================================
    MAP("open", my_open);
    MAP("close", my_close);
    MAP("read", my_read);
    MAP("write", my_write);
    MAP("ioctl", my_ioctl);

    // ============================================================
    // PROCESS / MISC STUBS
    // ============================================================
    MAP("getpid", _getpid);
    MAP("kill", my_stub_success);
    MAP("fork", my_stub_fail);
    MAP("daemon", my_stub_fail);
    MAP("wait", my_stub_success);
    MAP("execlp", my_stub_fail);

    MAP("shmget", my_stub_fail);
    MAP("shmat", my_stub_null);
    MAP("shmdt", my_stub_success);
    MAP("shmctl", my_stub_success);

    MAP("dlopen", my_stub_null);
    MAP("dlsym", my_stub_null);
    MAP("dlclose", my_stub_success);
    MAP("dlerror", my_stub_null);

    MAP("opendir", my_stub_null);
    MAP("readdir", my_stub_null);
    MAP("closedir", my_stub_success);

    MAP("tcgetattr", my_stub_success);
    MAP("tcsetattr", my_stub_success);
    MAP("tcflush", my_stub_success);
    MAP("tcdrain", my_stub_success);
    MAP("cfgetispeed", my_stub_success);
    MAP("cfgetospeed", my_stub_success);
    MAP("cfsetspeed", my_stub_success);

    MAP("iopl", my_stub_success);
    MAP("writev", my_stub_success);
    MAP("fcntl", my_stub_success);

    MAP("stat", my_stub_success);
    MAP("__xstat", my_stub_success);
    MAP("__lxstat", my_stub_success);
    MAP("__fxstat64", my_stub_success);
    MAP("__fxstat", LibcBridge::fxstat_wrapper);

    // ============================================================
    // NETWORK
    // ============================================================
    MAP("socket", socket);
    MAP("connect", connect);
    MAP("bind", bind);
    MAP("listen", listen);
    MAP("accept", accept);
    MAP("send", send);
    MAP("recv", recv);
    MAP("sendto", sendto);
    MAP("recvfrom", recvfrom);
    MAP("shutdown", shutdown);
    MAP("setsockopt", setsockopt);
    MAP("getsockopt", getsockopt);
    MAP("poll", my_stub_success);
    MAP("select", select);

    MAP("inet_addr", inet_addr);
    MAP("inet_ntoa", inet_ntoa);
    MAP("inet_aton", LibcBridge::inet_aton_wrapper);
    MAP("htons", htons);
    MAP("htonl", htonl);
    MAP("ntohs", ntohs);
    MAP("ntohl", ntohl);
    MAP("gethostbyname_r", LibcBridge::gethostbyname_r_wrapper);
    MAP("gethostbyaddr_r", LibcBridge::gethostbyaddr_r_wrapper);

    // ============================================================
    // SEGAAPI
    // ============================================================
    if (strncmp(name, "SEGAAPI_", 8) == 0) {
        if (strcmp(name, "SEGAAPI_Init") == 0) return (uintptr_t)&SEGAAPI_Init;
        if (strcmp(name, "SEGAAPI_Exit") == 0) return (uintptr_t)&SEGAAPI_Exit;
        if (strcmp(name, "SEGAAPI_Reset") == 0) return (uintptr_t)&SEGAAPI_Reset;

        // SEGAAPI - PLAYBACK CONTROL
        if (strcmp(name, "SEGAAPI_Play") == 0) return (uintptr_t)&SEGAAPI_Play;
        if (strcmp(name, "SEGAAPI_Pause") == 0) return (uintptr_t)&SEGAAPI_Pause;
        if (strcmp(name, "SEGAAPI_Stop") == 0) return (uintptr_t)&SEGAAPI_Stop;
        if (strcmp(name, "SEGAAPI_PlayWithSetup") == 0) return (uintptr_t)&SEGAAPI_PlayWithSetup;
        if (strcmp(name, "SEGAAPI_GetPlaybackStatus") == 0) return (uintptr_t)&SEGAAPI_GetPlaybackStatus;
        if (strcmp(name, "SEGAAPI_SetPlaybackPosition") == 0) return (uintptr_t)&SEGAAPI_SetPlaybackPosition;
        if (strcmp(name, "SEGAAPI_GetPlaybackPosition") == 0) return (uintptr_t)&SEGAAPI_GetPlaybackPosition;
        if (strcmp(name, "SEGAAPI_SetReleaseState") == 0) return (uintptr_t)&SEGAAPI_SetReleaseState;

        // SEGAAPI - BUFFER MANAGEMENT
        if (strcmp(name, "SEGAAPI_CreateBuffer") == 0) return (uintptr_t)&SEGAAPI_CreateBuffer;
        if (strcmp(name, "SEGAAPI_DestroyBuffer") == 0) return (uintptr_t)&SEGAAPI_DestroyBuffer;
        if (strcmp(name, "SEGAAPI_UpdateBuffer") == 0) return (uintptr_t)&SEGAAPI_UpdateBuffer;

        // SEGAAPI - FORMAT / SAMPLE RATE
        if (strcmp(name, "SEGAAPI_SetFormat") == 0) return (uintptr_t)&SEGAAPI_SetFormat;
        if (strcmp(name, "SEGAAPI_GetFormat") == 0) return (uintptr_t)&SEGAAPI_GetFormat;
        if (strcmp(name, "SEGAAPI_SetSampleRate") == 0) return (uintptr_t)&SEGAAPI_SetSampleRate;
        if (strcmp(name, "SEGAAPI_GetSampleRate") == 0) return (uintptr_t)&SEGAAPI_GetSampleRate;

        // SEGAAPI - PRIORITY / USER DATA
        if (strcmp(name, "SEGAAPI_SetPriority") == 0) return (uintptr_t)&SEGAAPI_SetPriority;
        if (strcmp(name, "SEGAAPI_GetPriority") == 0) return (uintptr_t)&SEGAAPI_GetPriority;
        if (strcmp(name, "SEGAAPI_SetUserData") == 0) return (uintptr_t)&SEGAAPI_SetUserData;
        if (strcmp(name, "SEGAAPI_GetUserData") == 0) return (uintptr_t)&SEGAAPI_GetUserData;

        // SEGAAPI - ROUTING / LEVELS
        if (strcmp(name, "SEGAAPI_SetSendRouting") == 0) return (uintptr_t)&SEGAAPI_SetSendRouting;
        if (strcmp(name, "SEGAAPI_GetSendRouting") == 0) return (uintptr_t)&SEGAAPI_GetSendRouting;
        if (strcmp(name, "SEGAAPI_SetSendLevel") == 0) return (uintptr_t)&SEGAAPI_SetSendLevel;
        if (strcmp(name, "SEGAAPI_GetSendLevel") == 0) return (uintptr_t)&SEGAAPI_GetSendLevel;
        if (strcmp(name, "SEGAAPI_SetChannelVolume") == 0) return (uintptr_t)&SEGAAPI_SetChannelVolume;
        if (strcmp(name, "SEGAAPI_GetChannelVolume") == 0) return (uintptr_t)&SEGAAPI_GetChannelVolume;

        // SEGAAPI - NOTIFICATIONS
        if (strcmp(name, "SEGAAPI_SetNotificationFrequency") == 0) return (uintptr_t)&SEGAAPI_SetNotificationFrequency;
        if (strcmp(name, "SEGAAPI_SetNotificationPoint") == 0) return (uintptr_t)&SEGAAPI_SetNotificationPoint;
        if (strcmp(name, "SEGAAPI_ClearNotificationPoint") == 0) return (uintptr_t)&SEGAAPI_ClearNotificationPoint;

        // SEGAAPI - LOOP CONTROL
        if (strcmp(name, "SEGAAPI_SetStartLoopOffset") == 0) return (uintptr_t)&SEGAAPI_SetStartLoopOffset;
        if (strcmp(name, "SEGAAPI_GetStartLoopOffset") == 0) return (uintptr_t)&SEGAAPI_GetStartLoopOffset;
        if (strcmp(name, "SEGAAPI_SetEndLoopOffset") == 0) return (uintptr_t)&SEGAAPI_SetEndLoopOffset;
        if (strcmp(name, "SEGAAPI_GetEndLoopOffset") == 0) return (uintptr_t)&SEGAAPI_GetEndLoopOffset;
        if (strcmp(name, "SEGAAPI_SetEndOffset") == 0) return (uintptr_t)&SEGAAPI_SetEndOffset;
        if (strcmp(name, "SEGAAPI_GetEndOffset") == 0) return (uintptr_t)&SEGAAPI_GetEndOffset;
        if (strcmp(name, "SEGAAPI_SetLoopState") == 0) return (uintptr_t)&SEGAAPI_SetLoopState;
        if (strcmp(name, "SEGAAPI_GetLoopState") == 0) return (uintptr_t)&SEGAAPI_GetLoopState;

        // SEGAAPI - SYNTH PARAMETERS
        if (strcmp(name, "SEGAAPI_SetSynthParam") == 0) return (uintptr_t)&SEGAAPI_SetSynthParam;
        if (strcmp(name, "SEGAAPI_GetSynthParam") == 0) return (uintptr_t)&SEGAAPI_GetSynthParam;
        if (strcmp(name, "SEGAAPI_SetSynthParamMultiple") == 0) return (uintptr_t)&SEGAAPI_SetSynthParamMultiple;
        if (strcmp(name, "SEGAAPI_GetSynthParamMultiple") == 0) return (uintptr_t)&SEGAAPI_GetSynthParamMultiple;

        // SEGAAPI - EAX
        if (strcmp(name, "SEGAAPI_SetGlobalEAXProperty") == 0) return (uintptr_t)&SEGAAPI_SetGlobalEAXProperty;
        if (strcmp(name, "SEGAAPI_GetGlobalEAXProperty") == 0) return (uintptr_t)&SEGAAPI_GetGlobalEAXProperty;

        // SEGAAPI - SPDIF
        if (strcmp(name, "SEGAAPI_SetSPDIFOutChannelStatus") == 0) return (uintptr_t)&SEGAAPI_SetSPDIFOutChannelStatus;
        if (strcmp(name, "SEGAAPI_GetSPDIFOutChannelStatus") == 0) return (uintptr_t)&SEGAAPI_GetSPDIFOutChannelStatus;
        if (strcmp(name, "SEGAAPI_SetSPDIFOutSampleRate") == 0) return (uintptr_t)&SEGAAPI_SetSPDIFOutSampleRate;
        if (strcmp(name, "SEGAAPI_GetSPDIFOutSampleRate") == 0) return (uintptr_t)&SEGAAPI_GetSPDIFOutSampleRate;
        if (strcmp(name, "SEGAAPI_SetSPDIFOutChannelRouting") == 0) return (uintptr_t)&SEGAAPI_SetSPDIFOutChannelRouting;
        if (strcmp(name, "SEGAAPI_GetSPDIFOutChannelRouting") == 0) return (uintptr_t)&SEGAAPI_GetSPDIFOutChannelRouting;

        // SEGAAPI - I/O VOLUME
        if (strcmp(name, "SEGAAPI_SetIOVolume") == 0) return (uintptr_t)&SEGAAPI_SetIOVolume;
        if (strcmp(name, "SEGAAPI_GetIOVolume") == 0) return (uintptr_t)&SEGAAPI_GetIOVolume;

        // SEGAAPI - STATUS
        if (strcmp(name, "SEGAAPI_SetLastStatus") == 0) return (uintptr_t)&SEGAAPI_SetLastStatus;
        if (strcmp(name, "SEGAAPI_GetLastStatus") == 0) return (uintptr_t)&SEGAAPI_GetLastStatus;
    }

    // ============================================================
    // GCC Exception Handling (_Unwind_*)
    // ============================================================
    if (strncmp(name, "_Unwind_", 8) == 0) {
        EnsureLibGccLoaded();
        if (g_hLibGcc) {
            void* proc = (void*)GetProcAddress(g_hLibGcc, name);
            if (proc) return (uintptr_t)proc;
        }
        std::cerr << "[Windy] _Unwind_ symbol not found in libgcc: " << name << std::endl;
    }

    // ============================================================
    // OPEN GL TODO: bridge to SDL2 or something else
    // ============================================================
    if (strncmp(name, "gl", 2) == 0 && strncmp(name, "glX", 3) != 0) {
        typedef void* (WINAPI* PFNWGLGETPROCADDRESS)(LPCSTR);
        static HMODULE hOpengl = LoadLibraryA("opengl32.dll");
        static PFNWGLGETPROCADDRESS my_wglGetProcAddress = (PFNWGLGETPROCADDRESS)GetProcAddress(hOpengl, "wglGetProcAddress");

        void* proc = NULL;
        if (my_wglGetProcAddress) proc = my_wglGetProcAddress(name);

        if (!proc) {
            proc = (void*)GetProcAddress(hOpengl, name);
        }
        if (proc) return (uintptr_t)proc;
    }

    // ============================================================
    // MSYS2 Fallback
    // ============================================================
    EnsureMsysLoaded();
    if (g_hMsys) {
        void* proc = (void*)GetProcAddress(g_hMsys, name);
        if (proc) return (uintptr_t)proc;
    }

    std::cout << "[WARNING] Symbol unresolved: " << name << " -> Stub" << std::endl;
    return (uintptr_t)&UnimplementedStub;
}

void SymbolResolver::ResolveAll(uintptr_t jmpRel, uintptr_t symTab, uintptr_t strTab, uint32_t pltRelSize) {
    if (!jmpRel || !symTab || !strTab) return;
    size_t relCount = pltRelSize / sizeof(Elf32_Rel);
    std::cout << "--- Starting Dynamic Symbol Resolution ---" << std::endl;

    for (size_t i = 0; i < relCount; ++i) {
        Elf32_Rel* rel = (Elf32_Rel*)(jmpRel + i * sizeof(Elf32_Rel));
        uint32_t symIdx = ELF32_R_SYM(rel->r_info);
        Elf32_Sym* sym = (Elf32_Sym*)(symTab + symIdx * sizeof(Elf32_Sym));
        const char* name = (const char*)(strTab + sym->st_name);
        uintptr_t addr = GetExternalAddr(name);
        DWORD oldProtect;
        VirtualProtect((LPVOID)rel->r_offset, 4, PAGE_READWRITE, &oldProtect);
        *(uintptr_t*)(rel->r_offset) = addr;
        VirtualProtect((LPVOID)rel->r_offset, 4, oldProtect, &oldProtect);
    }
}
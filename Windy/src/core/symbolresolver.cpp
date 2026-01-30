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
#include <errno.h>
#include <map>
#include <algorithm>
#include <vector>
#include <string>
#include <math.h>
#include <stdlib.h>

// --- OpenGL ---
#ifndef APIENTRY
#define APIENTRY __stdcall
#endif
#include <GL/gl.h>

// --- Pthreads & Semaphore ---
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>

#include "SymbolResolver.h"
#include "common.h"
#include "../src/core/log.h"

// --- Bridges ---
#include "../api/sega/libsegaapi.h"
#include "../api/graphics/X11Bridge.h"
#include "../api/graphics/GLXBridge.h"
#include "../api/libc/LibcBridge.h"
#include "../api/libc/PthreadBridge.h"

// --- Linux Bridges ---
#include "../api/linux/LinuxTypes.h"
#include "../api/linux/GccBridge.h"
#include "../api/linux/IpcBridge.h"
#include "../api/linux/ProcessBridge.h"
#include "../api/linux/FilesystemBridge.h"
#include "../api/linux/HardwareBridge.h"

// --- Helper Macros ---
#define ELF32_R_SYM(val) ((val) >> 8)

// --- GPU Spoofing (OpenGL) ---
static const char* GPU_VENDOR_STRING = "NVIDIA Corporation";
static const char* GPU_RENDERER_STRING = "GeForce 7800/PCIe/SSE2";
static const char* GPU_VERSION_STRING = "2.1.2 NVIDIA 285.05.09";
static const char* GPU_SL_VERSION_STRING = "1.20 NVIDIA via Cg compiler";

// =============================================================
//   Stubs & Utilities
// =============================================================

extern "C" int my_stub_success() { return 0; }
extern "C" int my_stub_fail() { return -1; }
extern "C" void* my_stub_null() { return NULL; }

// =============================================================
//   Semaphore Wrappers with Logging
// =============================================================

extern "C" int my_sem_init(sem_t* sem, int pshared, unsigned int value) {
    log_info(">>> sem_init: sem=%p, pshared=%d, value=%u", sem, pshared, value);
    int ret = sem_init(sem, pshared, value);
    log_info(">>> sem_init EXIT: ret=%d", ret);
    return ret;
}

extern "C" int my_sem_wait(sem_t* sem) {
    log_info(">>> sem_wait ENTRY: sem=%p (THIS MAY BLOCK!)", sem);
    int ret = sem_wait(sem);
    log_info(">>> sem_wait EXIT: ret=%d", ret);
    return ret;
}

extern "C" int my_sem_trywait(sem_t* sem) {
    log_debug(">>> sem_trywait: sem=%p", sem);
    int ret = sem_trywait(sem);
    log_debug(">>> sem_trywait EXIT: ret=%d", ret);
    return ret;
}

extern "C" int my_sem_post(sem_t* sem) {
    log_debug(">>> sem_post: sem=%p", sem);
    int ret = sem_post(sem);
    return ret;
}

void __declspec(naked) UnimplementedStub() {
    __asm { pushad }
    MessageBoxA(NULL, "Unimplemented Function Called", "Error", MB_OK);
    TerminateProcess(GetCurrentProcess(), 1);
}

int ExceptionFilter(unsigned int code, struct _EXCEPTION_POINTERS* ep) {
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// GPU Spoofing
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

// --- GLUT Support ---
void (*g_glutKeyboardFunc)(unsigned char key, int x, int y) = nullptr;

extern "C" void my_glutKeyboardFunc(void (*func)(unsigned char key, int x, int y)) {
    g_glutKeyboardFunc = func;
}

// --- Missing Libc / System Functions ---

extern "C" void my_bzero(void* s, size_t n) {
    memset(s, 0, n);
}

extern "C" int my_bcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

extern "C" int my_clock_gettime(int clk_id, struct timespec* tp) {
    if (!tp) return -1;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned __int64 t = (unsigned __int64)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
    // 1601 to 1970 epoch
    t -= 116444736000000000ULL;
    tp->tv_sec = (time_t)(t / 10000000);
    tp->tv_nsec = (long)((t % 10000000) * 100);
    return 0;
}

// --- File I/O Wrappers ---

extern "C" int my_fsync(int fd) { return _commit(fd); }
extern "C" int my_fdatasync(int fd) { return _commit(fd); }

extern "C" int my_access(const char* pathname, int mode) {
    if (pathname && strstr(pathname, "/dev/") != NULL) {
        return 0; // Success
    }
    if (mode == 1) mode = 0;
    return _access(pathname, mode);
}

extern "C" int my_chmod(const char* filename, int pmode) { return _chmod(filename, pmode); }
extern "C" long my_lseek(int fd, long offset, int origin) { return _lseek(fd, offset, origin); }

// Linux __xstat64 -> Windows _stat64
extern "C" int my_xstat64(int ver, const char* path, struct _stat64* buffer) {
    return _stat64(path, buffer);
}

// --- Env Wrappers ---

extern "C" int my_setenv(const char* name, const char* value, int overwrite) {
    if (!overwrite && getenv(name)) return 0;
    return _putenv_s(name, value);
}

extern "C" int my_unsetenv(const char* name) {
    return _putenv_s(name, "");
}

extern "C" int my_putenv(char* string) {
    return _putenv(string);
}

// --- Select Wrapper ---
// Handles timeout-only usage (common in loops) or logs usage with FDs
extern "C" int my_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout) {
    log_info(">>> select called: nfds=%d, timeout=%s", nfds, timeout ? "set" : "NULL");
    if (!readfds && !writefds && !exceptfds && timeout) {
        DWORD ms = (timeout->tv_sec * 1000) + (timeout->tv_usec / 1000);
        log_info(">>> select: sleep-only mode, sleeping %lu ms", ms);
        Sleep(ms);
        log_info(">>> select EXIT: returning 0");
        return 0;
    }
    log_info(">>> select: calling real select...");
    int ret = select(nfds, readfds, writefds, exceptfds, timeout);
    log_info(">>> select EXIT: returning %d", ret);
    return ret;
}

// =============================================================
//   Poll Wrapper (with logging)
// =============================================================

// Linux pollfd structure
struct linux_pollfd {
    int fd;
    short events;
    short revents;
};

#define LINUX_POLLIN   0x0001
#define LINUX_POLLOUT  0x0004
#define LINUX_POLLERR  0x0008
#define LINUX_POLLHUP  0x0010
#define LINUX_POLLNVAL 0x0020

extern "C" int my_poll(struct linux_pollfd* fds, unsigned long nfds, int timeout) {
    log_info(">>> poll called: nfds=%lu, timeout=%d", nfds, timeout);

    for (unsigned long i = 0; i < nfds && i < 5; i++) {
        log_info(">>>   poll fd[%lu]: fd=%d, events=0x%04X", i, fds[i].fd, fds[i].events);
    }
    if (nfds > 5) {
        log_info(">>>   ... and %lu more fds", nfds - 5);
    }

    // If no fds, just sleep
    if (nfds == 0) {
        if (timeout > 0) {
            Sleep(timeout);
        }
        log_info(">>> poll EXIT: returning 0 (no fds)");
        return 0;
    }

    // Convert to Windows select
    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    int maxfd = 0;
    bool hasValidFd = false;

    for (unsigned long i = 0; i < nfds; i++) {
        fds[i].revents = 0;

        if (fds[i].fd < 0) {
            continue;
        }

        hasValidFd = true;
        if (fds[i].fd > maxfd) maxfd = fds[i].fd;

        if (fds[i].events & LINUX_POLLIN) {
            FD_SET((SOCKET)fds[i].fd, &readfds);
        }
        if (fds[i].events & LINUX_POLLOUT) {
            FD_SET((SOCKET)fds[i].fd, &writefds);
        }
        FD_SET((SOCKET)fds[i].fd, &exceptfds);
    }

    if (!hasValidFd) {
        if (timeout > 0) {
            Sleep(timeout);
        }
        log_info(">>> poll EXIT: returning 0 (no valid fds)");
        return 0;
    }

    struct timeval tv;
    struct timeval* tvp = NULL;
    if (timeout >= 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        tvp = &tv;
    }

    log_info(">>> poll: calling select (maxfd=%d)...", maxfd);
    int ret = select(maxfd + 1, &readfds, &writefds, &exceptfds, tvp);
    log_info(">>> poll: select returned %d", ret);

    if (ret > 0) {
        int count = 0;
        for (unsigned long i = 0; i < nfds; i++) {
            if (fds[i].fd < 0) continue;

            if (FD_ISSET(fds[i].fd, &readfds)) {
                fds[i].revents |= LINUX_POLLIN;
            }
            if (FD_ISSET(fds[i].fd, &writefds)) {
                fds[i].revents |= LINUX_POLLOUT;
            }
            if (FD_ISSET(fds[i].fd, &exceptfds)) {
                fds[i].revents |= LINUX_POLLERR;
            }
            if (fds[i].revents) count++;
        }
        log_info(">>> poll EXIT: returning %d", count);
        return count;
    }

    log_info(">>> poll EXIT: returning %d", ret);
    return ret;
}

// =============================================================
//   Socket Wrappers (with logging)
// =============================================================

extern "C" SOCKET my_socket(int af, int type, int protocol) {
    log_info(">>> socket called: af=%d, type=%d, protocol=%d", af, type, protocol);
    SOCKET s = socket(af, type, protocol);
    log_info(">>> socket EXIT: returning %lld", (long long)s);
    return s;
}

extern "C" int my_connect(SOCKET s, const struct sockaddr* name, int namelen) {
    log_info(">>> connect called: socket=%lld", (long long)s);
    int ret = connect(s, name, namelen);
    log_info(">>> connect EXIT: returning %d (WSAError=%d)", ret, ret < 0 ? WSAGetLastError() : 0);
    return ret;
}

extern "C" int my_bind(SOCKET s, const struct sockaddr* name, int namelen) {
    log_info(">>> bind called: socket=%lld", (long long)s);
    int ret = bind(s, name, namelen);
    log_info(">>> bind EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_listen(SOCKET s, int backlog) {
    log_info(">>> listen called: socket=%lld, backlog=%d", (long long)s, backlog);
    int ret = listen(s, backlog);
    log_info(">>> listen EXIT: returning %d", ret);
    return ret;
}

extern "C" SOCKET my_accept(SOCKET s, struct sockaddr* addr, int* addrlen) {
    log_info(">>> accept ENTRY: socket=%lld (THIS MAY BLOCK!)", (long long)s);
    SOCKET ret = accept(s, addr, addrlen);
    log_info(">>> accept EXIT: returning %lld (WSAError=%d)", (long long)ret, ret == INVALID_SOCKET ? WSAGetLastError() : 0);
    return ret;
}

extern "C" int my_recv(SOCKET s, char* buf, int len, int flags) {
    log_info(">>> recv ENTRY: socket=%lld, len=%d (THIS MAY BLOCK!)", (long long)s, len);
    int ret = recv(s, buf, len, flags);
    log_info(">>> recv EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_send(SOCKET s, const char* buf, int len, int flags) {
    log_info(">>> send called: socket=%lld, len=%d", (long long)s, len);
    int ret = send(s, buf, len, flags);
    log_info(">>> send EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_recvfrom(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen) {
    log_info(">>> recvfrom ENTRY: socket=%lld, len=%d (THIS MAY BLOCK!)", (long long)s, len);
    int ret = recvfrom(s, buf, len, flags, from, fromlen);
    log_info(">>> recvfrom EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_sendto(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) {
    log_info(">>> sendto called: socket=%lld, len=%d", (long long)s, len);
    int ret = sendto(s, buf, len, flags, to, tolen);
    log_info(">>> sendto EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_setsockopt(SOCKET s, int level, int optname, const char* optval, int optlen) {
    log_info(">>> setsockopt called: socket=%lld, level=%d, optname=%d", (long long)s, level, optname);
    int ret = setsockopt(s, level, optname, optval, optlen);
    log_info(">>> setsockopt EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_getsockopt(SOCKET s, int level, int optname, char* optval, int* optlen) {
    log_info(">>> getsockopt called: socket=%lld, level=%d, optname=%d", (long long)s, level, optname);
    int ret = getsockopt(s, level, optname, optval, optlen);
    log_info(">>> getsockopt EXIT: returning %d", ret);
    return ret;
}

// --- Lindbergh Specific Stubs ---

extern "C" void my_kswap_collect(void* p) { return; }

// Entrypoint hooking
typedef int (*MainFunc)(int, char**, char**);
extern "C" void my_libc_start_main(MainFunc m, int c, char** a, void (*i)(), void (*f)(), void (*r)(), void* s) {
    log_info("__libc_start_main called");
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (i) {
        log_debug("Calling init function");
        __try { i(); }
        __except (ExceptionFilter(GetExceptionCode(), GetExceptionInformation())) {
            log_error("Exception in init function");
        }
    }
    int result = 0;
    log_info("Calling main(argc=%d)", c);
    __try { result = m(c, a, nullptr); }
    __except (ExceptionFilter(GetExceptionCode(), GetExceptionInformation())) {
        log_fatal("Exception in main() - process terminated");
        exit(1);
    }
    log_info("main() returned %d", result);
    exit(result);
}

// =============================================================
//   Symbol Resolver Main Function
// =============================================================
uintptr_t SymbolResolver::GetExternalAddr(const char* name) {

    // Log symbol resolution requests for debugging (only unresolved ones are logged as warnings)
    // log_trace("Resolving symbol: %s", name);

    // Special Handling
    if (strcmp(name, "glGetString") == 0) return (uintptr_t)&my_glGetString;

    // NVIDIA Cg Toolkit
    if (strncmp(name, "cg", 2) == 0) {
        static HMODULE hCg = NULL;
        static HMODULE hCgGL = NULL;
        static bool attemptedLoad = false;
        if (!attemptedLoad) {
            hCg = LoadLibraryA("cg.dll");
            if (hCg) log_info("Loaded cg.dll");
            hCgGL = LoadLibraryA("cgGL.dll");
            if (hCgGL) log_info("Loaded cgGL.dll");
            attemptedLoad = true;
        }
        void* proc = NULL;
        if (hCg) proc = (void*)GetProcAddress(hCg, name);
        if (!proc && hCgGL) proc = (void*)GetProcAddress(hCgGL, name);
        if (proc) return (uintptr_t)proc;
    }

    // Standard Mapping Macro
#define MAP(n, f) if (strcmp(name, n) == 0) return (uintptr_t)&f
    // Overloaded Function Mapping Macro (No & operator)
#define MAP_OL(n, f) if (strcmp(name, n) == 0) return (uintptr_t)(f)

    // ============================================================
    // GLUT / GLU
    // ============================================================
    MAP("glutInit", GLXBridge::glutInit);
    MAP("glutInitDisplayMode", GLXBridge::glutInitDisplayMode);
    MAP("glutInitWindowSize", GLXBridge::glutInitWindowSize);
    MAP("glutInitWindowPosition", GLXBridge::glutInitWindowPosition);
    MAP("glutEnterGameMode", GLXBridge::glutEnterGameMode);
    MAP("glutLeaveGameMode", GLXBridge::glutLeaveGameMode);
    MAP("glutMainLoop", GLXBridge::glutMainLoop);
    MAP("glutDisplayFunc", GLXBridge::glutDisplayFunc);
    MAP("glutIdleFunc", GLXBridge::glutIdleFunc);
    MAP("glutPostRedisplay", GLXBridge::glutPostRedisplay);
    MAP("glutSwapBuffers", GLXBridge::glutSwapBuffers);
    MAP("glutGet", GLXBridge::glutGet);
    MAP("glutSetCursor", GLXBridge::glutSetCursor);
    MAP("glutGameModeString", GLXBridge::glutGameModeString);
    MAP("glutBitmapCharacter", GLXBridge::glutBitmapCharacter);
    MAP("glutBitmapWidth", GLXBridge::glutBitmapWidth);
    MAP("glutKeyboardFunc", my_glutKeyboardFunc);

    MAP("gluPerspective", GLXBridge::gluPerspective);
    MAP("gluLookAt", GLXBridge::gluLookAt);

    // ============================================================
    // Missing Libc / Linux Specific / File I/O
    // ============================================================
    MAP("bzero", my_bzero);
    MAP("bcmp", my_bcmp);
    MAP("clock_gettime", my_clock_gettime);

    // File I/O
    MAP("fsync", my_fsync);
    MAP("fdatasync", my_fdatasync);
    MAP("lseek", my_lseek);
    MAP("access", my_access);
    MAP("chmod", my_chmod);
    MAP("__xstat64", my_xstat64);

    // Env
    MAP("setenv", my_setenv);
    MAP("unsetenv", my_unsetenv);
    MAP("putenv", my_putenv);

    // Network / Select
    MAP("select", my_select);

    // Lindbergh Specific
    MAP("kswap_collect", my_kswap_collect);

    // ============================================================
    // Math Functions (Explicit Casting & MAP_OL macro)
    // ============================================================
    MAP_OL("atan", (double(*)(double))atan);
    MAP_OL("atan2", (double(*)(double, double))atan2);
    MAP_OL("cos", (double(*)(double))cos);
    MAP_OL("sin", (double(*)(double))sin);
    MAP_OL("tan", (double(*)(double))tan);
    MAP_OL("sqrt", (double(*)(double))sqrt);
    MAP_OL("pow", (double(*)(double, double))pow);
    MAP_OL("powf", (double(*)(double, double))pow); // Map powf to pow
    MAP_OL("floor", (double(*)(double))floor);
    MAP_OL("ceil", (double(*)(double))ceil);
    MAP_OL("fmod", (double(*)(double, double))fmod);
    MAP_OL("abs", (int(*)(int))abs);
    MAP_OL("fabs", (double(*)(double))fabs);

    // ============================================================
    // GCC / GLIBC Internals
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

    // ============================================================
    // Hardware / Low-Level I/O
    // ============================================================
    MAP("open", my_open);
    MAP("open64", my_open);
    MAP("close", my_close);
    MAP("read", my_read);
    MAP("write", my_write);
    MAP("ioctl", my_ioctl);
    MAP("writev", my_writev);
    MAP("openat", my_openat);
    MAP("openat64", my_openat);
    MAP("__xstat64", __xstat64);

    MAP("__open_2", my_open);
    MAP("__open64_2", my_open);
    MAP("__openat_2", my_openat);
    MAP("__openat64_2", my_openat);

    // ============================================================
    // IPC
    // ============================================================
    MAP("shmget", my_shmget);
    MAP("shmat", my_shmat);
    MAP("shmctl", my_shmctl);
    MAP("shmdt", my_shmdt);

    // ============================================================
    // Process
    // ============================================================
    MAP("getpid", _getpid);
    MAP("fork", my_fork);
    MAP("vfork", my_vfork);
    MAP("daemon", my_daemon);
    MAP("execlp", my_execlp);
    MAP("kill", my_stub_success);
    MAP("wait", my_stub_success);

    // ============================================================
    // Filesystem
    // ============================================================
    MAP("dlopen", my_dlopen);
    MAP("dlsym", my_dlsym);
    MAP("dlclose", my_dlclose);
    MAP("dlerror", my_dlerror);

    MAP("opendir", my_opendir);
    MAP("readdir", my_readdir);
    MAP("closedir", my_closedir);
    MAP("rewinddir", my_rewinddir);

    MAP("stat", my_stat);
    MAP("fstat", my_fstat);
    MAP("lstat", my_lstat);
    MAP("__xstat", __xstat);
    MAP("__lxstat", __lxstat);
    MAP("__fxstat", __fxstat);
    MAP("__fxstat64", __fxstat64);
    MAP("__xmknod", my_stub_success);
    MAP("fcntl", my_fcntl);

    // ============================================================
    // Libc Wrappers
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

    // String & Memory
    MAP("sprintf", LibcBridge::sprintf_wrapper);
    MAP("snprintf", LibcBridge::snprintf_wrapper);
    MAP("vsprintf", LibcBridge::vsprintf_wrapper);
    MAP("vsnprintf", LibcBridge::vsnprintf_wrapper);
    MAP("sscanf", LibcBridge::sscanf_wrapper);

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

    MAP("memchr", LibcBridge::memchr_wrapper);
    MAP("memmove", LibcBridge::memmove_wrapper);
    MAP("malloc", LibcBridge::malloc_wrapper);
    MAP("free", LibcBridge::free_wrapper);
    MAP("calloc", LibcBridge::calloc_wrapper);
    MAP("realloc", LibcBridge::realloc_wrapper);
    MAP("memalign", LibcBridge::memalign_wrapper);
    MAP("memcpy", memcpy);
    MAP("memset", memset);

    // Path / Environment
    MAP("getcwd", LibcBridge::getcwd_wrapper);
    MAP("realpath", LibcBridge::realpath_wrapper);
    MAP("remove", LibcBridge::remove_wrapper);
    MAP("mkdir", _mkdir);
    MAP("rmdir", _rmdir);
    MAP("unlink", _unlink);
    MAP("access", _access);
    MAP("atoi", atoi);
    MAP("rand", LibcBridge::rand_wrapper);
    MAP("srand", LibcBridge::srand_wrapper);
    MAP("getenv", LibcBridge::getenv_wrapper);
    MAP("system", LibcBridge::system_wrapper);
    MAP("abort", LibcBridge::abort_wrapper);
    MAP("raise", LibcBridge::raise_wrapper);
    MAP("exit", LibcBridge::exit_wrapper);
    MAP("_exit", LibcBridge::_exit_wrapper);

    // Time
    MAP("time", time);
    MAP("utime", LibcBridge::utime_wrapper);
    MAP("gettimeofday", LibcBridge::gettimeofday_wrapper);
    MAP("usleep", LibcBridge::usleep_wrapper);
    MAP("sleep", LibcBridge::sleep_wrapper);
    MAP("nanosleep", LibcBridge::nanosleep_wrapper);
    MAP("localtime", LibcBridge::localtime_wrapper);
    MAP("localtime_r", LibcBridge::localtime_r_wrapper);
    MAP("mktime", LibcBridge::mktime_wrapper);
    MAP("strftime", LibcBridge::strftime_wrapper);
    MAP("settimeofday", LibcBridge::settimeofday_wrapper);

    // Locale / WideChar
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
    // Network / Socket (with logging wrappers)
    // ============================================================
    MAP("socket", my_socket);
    MAP("connect", my_connect);
    MAP("bind", my_bind);
    MAP("listen", my_listen);
    MAP("accept", my_accept);
    MAP("send", my_send);
    MAP("recv", my_recv);
    MAP("sendto", my_sendto);
    MAP("recvfrom", my_recvfrom);
    MAP("shutdown", shutdown);
    MAP("setsockopt", my_setsockopt);
    MAP("getsockopt", my_getsockopt);
    // select mapped above to my_select
    MAP("inet_addr", inet_addr);
    MAP("inet_ntoa", inet_ntoa);
    MAP("inet_aton", LibcBridge::inet_aton_wrapper);
    MAP("htons", htons);
    MAP("htonl", htonl);
    MAP("ntohs", ntohs);
    MAP("ntohl", ntohl);
    MAP("gethostbyname_r", LibcBridge::gethostbyname_r_wrapper);
    MAP("gethostbyaddr_r", LibcBridge::gethostbyaddr_r_wrapper);

    // Stubs (poll now has proper implementation)
    MAP("poll", my_poll);
    MAP("tcgetattr", my_stub_success);
    MAP("tcsetattr", my_stub_success);
    MAP("tcflush", my_stub_success);
    MAP("tcdrain", my_stub_success);
    MAP("cfgetispeed", my_stub_success);
    MAP("cfgetospeed", my_stub_success);
    MAP("cfsetspeed", my_stub_success);
    MAP("iopl", my_stub_success);

    // Pthreads
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
    MAP("pthread_cond_init", PthreadBridge::cond_init_wrapper);
    MAP("pthread_cond_destroy", PthreadBridge::cond_destroy_wrapper);
    MAP("pthread_cond_wait", PthreadBridge::cond_wait_wrapper);
    MAP("pthread_cond_timedwait", PthreadBridge::cond_timedwait_wrapper);
    MAP("pthread_cond_signal", PthreadBridge::cond_signal_wrapper);
    MAP("pthread_cond_broadcast", PthreadBridge::cond_broadcast_wrapper);
    MAP("pthread_key_create", pthread_key_create);
    MAP("pthread_key_delete", pthread_key_delete);
    MAP("pthread_setspecific", pthread_setspecific);
    MAP("pthread_getspecific", pthread_getspecific);
    MAP("sem_init", my_sem_init);
    MAP("sem_destroy", sem_destroy);
    MAP("sem_wait", my_sem_wait);
    MAP("sem_trywait", my_sem_trywait);
    MAP("sem_post", my_sem_post);
    MAP("sem_getvalue", sem_getvalue);

    // Sega API
    if (strncmp(name, "SEGAAPI_", 8) == 0) {
        if (strcmp(name, "SEGAAPI_Init") == 0) return (uintptr_t)&SEGAAPI_Init;
        if (strcmp(name, "SEGAAPI_Exit") == 0) return (uintptr_t)&SEGAAPI_Exit;
        if (strcmp(name, "SEGAAPI_Reset") == 0) return (uintptr_t)&SEGAAPI_Reset;
        if (strcmp(name, "SEGAAPI_Play") == 0) return (uintptr_t)&SEGAAPI_Play;
        if (strcmp(name, "SEGAAPI_Pause") == 0) return (uintptr_t)&SEGAAPI_Pause;
        if (strcmp(name, "SEGAAPI_Stop") == 0) return (uintptr_t)&SEGAAPI_Stop;
        if (strcmp(name, "SEGAAPI_PlayWithSetup") == 0) return (uintptr_t)&SEGAAPI_PlayWithSetup;
        if (strcmp(name, "SEGAAPI_GetPlaybackStatus") == 0) return (uintptr_t)&SEGAAPI_GetPlaybackStatus;
        if (strcmp(name, "SEGAAPI_SetPlaybackPosition") == 0) return (uintptr_t)&SEGAAPI_SetPlaybackPosition;
        if (strcmp(name, "SEGAAPI_GetPlaybackPosition") == 0) return (uintptr_t)&SEGAAPI_GetPlaybackPosition;
        if (strcmp(name, "SEGAAPI_SetReleaseState") == 0) return (uintptr_t)&SEGAAPI_SetReleaseState;
        if (strcmp(name, "SEGAAPI_CreateBuffer") == 0) return (uintptr_t)&SEGAAPI_CreateBuffer;
        if (strcmp(name, "SEGAAPI_DestroyBuffer") == 0) return (uintptr_t)&SEGAAPI_DestroyBuffer;
        if (strcmp(name, "SEGAAPI_UpdateBuffer") == 0) return (uintptr_t)&SEGAAPI_UpdateBuffer;
        if (strcmp(name, "SEGAAPI_SetFormat") == 0) return (uintptr_t)&SEGAAPI_SetFormat;
        if (strcmp(name, "SEGAAPI_GetFormat") == 0) return (uintptr_t)&SEGAAPI_GetFormat;
        if (strcmp(name, "SEGAAPI_SetSampleRate") == 0) return (uintptr_t)&SEGAAPI_SetSampleRate;
        if (strcmp(name, "SEGAAPI_GetSampleRate") == 0) return (uintptr_t)&SEGAAPI_GetSampleRate;
        if (strcmp(name, "SEGAAPI_SetPriority") == 0) return (uintptr_t)&SEGAAPI_SetPriority;
        if (strcmp(name, "SEGAAPI_GetPriority") == 0) return (uintptr_t)&SEGAAPI_GetPriority;
        if (strcmp(name, "SEGAAPI_SetUserData") == 0) return (uintptr_t)&SEGAAPI_SetUserData;
        if (strcmp(name, "SEGAAPI_GetUserData") == 0) return (uintptr_t)&SEGAAPI_GetUserData;
        if (strcmp(name, "SEGAAPI_SetSendRouting") == 0) return (uintptr_t)&SEGAAPI_SetSendRouting;
        if (strcmp(name, "SEGAAPI_GetSendRouting") == 0) return (uintptr_t)&SEGAAPI_GetSendRouting;
        if (strcmp(name, "SEGAAPI_SetSendLevel") == 0) return (uintptr_t)&SEGAAPI_SetSendLevel;
        if (strcmp(name, "SEGAAPI_GetSendLevel") == 0) return (uintptr_t)&SEGAAPI_GetSendLevel;
        if (strcmp(name, "SEGAAPI_SetChannelVolume") == 0) return (uintptr_t)&SEGAAPI_SetChannelVolume;
        if (strcmp(name, "SEGAAPI_GetChannelVolume") == 0) return (uintptr_t)&SEGAAPI_GetChannelVolume;
        if (strcmp(name, "SEGAAPI_SetNotificationFrequency") == 0) return (uintptr_t)&SEGAAPI_SetNotificationFrequency;
        if (strcmp(name, "SEGAAPI_SetNotificationPoint") == 0) return (uintptr_t)&SEGAAPI_SetNotificationPoint;
        if (strcmp(name, "SEGAAPI_ClearNotificationPoint") == 0) return (uintptr_t)&SEGAAPI_ClearNotificationPoint;
        if (strcmp(name, "SEGAAPI_SetStartLoopOffset") == 0) return (uintptr_t)&SEGAAPI_SetStartLoopOffset;
        if (strcmp(name, "SEGAAPI_GetStartLoopOffset") == 0) return (uintptr_t)&SEGAAPI_GetStartLoopOffset;
        if (strcmp(name, "SEGAAPI_SetEndLoopOffset") == 0) return (uintptr_t)&SEGAAPI_SetEndLoopOffset;
        if (strcmp(name, "SEGAAPI_GetEndLoopOffset") == 0) return (uintptr_t)&SEGAAPI_GetEndLoopOffset;
        if (strcmp(name, "SEGAAPI_SetEndOffset") == 0) return (uintptr_t)&SEGAAPI_SetEndOffset;
        if (strcmp(name, "SEGAAPI_GetEndOffset") == 0) return (uintptr_t)&SEGAAPI_GetEndOffset;
        if (strcmp(name, "SEGAAPI_SetLoopState") == 0) return (uintptr_t)&SEGAAPI_SetLoopState;
        if (strcmp(name, "SEGAAPI_GetLoopState") == 0) return (uintptr_t)&SEGAAPI_GetLoopState;
        if (strcmp(name, "SEGAAPI_SetSynthParam") == 0) return (uintptr_t)&SEGAAPI_SetSynthParam;
        if (strcmp(name, "SEGAAPI_GetSynthParam") == 0) return (uintptr_t)&SEGAAPI_GetSynthParam;
        if (strcmp(name, "SEGAAPI_SetSynthParamMultiple") == 0) return (uintptr_t)&SEGAAPI_SetSynthParamMultiple;
        if (strcmp(name, "SEGAAPI_GetSynthParamMultiple") == 0) return (uintptr_t)&SEGAAPI_GetSynthParamMultiple;
        if (strcmp(name, "SEGAAPI_SetGlobalEAXProperty") == 0) return (uintptr_t)&SEGAAPI_SetGlobalEAXProperty;
        if (strcmp(name, "SEGAAPI_GetGlobalEAXProperty") == 0) return (uintptr_t)&SEGAAPI_GetGlobalEAXProperty;
        if (strcmp(name, "SEGAAPI_SetSPDIFOutChannelStatus") == 0) return (uintptr_t)&SEGAAPI_SetSPDIFOutChannelStatus;
        if (strcmp(name, "SEGAAPI_GetSPDIFOutChannelStatus") == 0) return (uintptr_t)&SEGAAPI_GetSPDIFOutChannelStatus;
        if (strcmp(name, "SEGAAPI_SetSPDIFOutSampleRate") == 0) return (uintptr_t)&SEGAAPI_SetSPDIFOutSampleRate;
        if (strcmp(name, "SEGAAPI_GetSPDIFOutSampleRate") == 0) return (uintptr_t)&SEGAAPI_GetSPDIFOutSampleRate;
        if (strcmp(name, "SEGAAPI_SetSPDIFOutChannelRouting") == 0) return (uintptr_t)&SEGAAPI_SetSPDIFOutChannelRouting;
        if (strcmp(name, "SEGAAPI_GetSPDIFOutChannelRouting") == 0) return (uintptr_t)&SEGAAPI_GetSPDIFOutChannelRouting;
        if (strcmp(name, "SEGAAPI_SetIOVolume") == 0) return (uintptr_t)&SEGAAPI_SetIOVolume;
        if (strcmp(name, "SEGAAPI_GetIOVolume") == 0) return (uintptr_t)&SEGAAPI_GetIOVolume;
        if (strcmp(name, "SEGAAPI_SetLastStatus") == 0) return (uintptr_t)&SEGAAPI_SetLastStatus;
        if (strcmp(name, "SEGAAPI_GetLastStatus") == 0) return (uintptr_t)&SEGAAPI_GetLastStatus;
    }

    // Exception Handling (_Unwind_*)
    if (strncmp(name, "_Unwind_", 8) == 0) {
        void* proc = GetLibGccSymbol(name);
        if (proc) return (uintptr_t)proc;
    }

    // OpenGL (Direct)
    if (strncmp(name, "gl", 2) == 0 && strncmp(name, "glX", 3) != 0 && strncmp(name, "glut", 4) != 0) {
        typedef void* (WINAPI* PFNWGLGETPROCADDRESS)(LPCSTR);
        static HMODULE hOpengl = LoadLibraryA("opengl32.dll");
        static PFNWGLGETPROCADDRESS my_wglGetProcAddress = (PFNWGLGETPROCADDRESS)GetProcAddress(hOpengl, "wglGetProcAddress");
        void* proc = NULL;
        if (my_wglGetProcAddress) proc = my_wglGetProcAddress(name);
        if (!proc) proc = (void*)GetProcAddress(hOpengl, name);
        if (proc) return (uintptr_t)proc;
    }

    // MSYS2 Fallback
    void* msysProc = GetMsysSymbol(name);
    if (msysProc) return (uintptr_t)msysProc;

    log_warn("Symbol unresolved: %s -> Using stub", name);
    return (uintptr_t)&UnimplementedStub;
}

void SymbolResolver::ResolveAll(uintptr_t jmpRel, uintptr_t symTab, uintptr_t strTab, uint32_t pltRelSize) {
    if (!jmpRel || !symTab || !strTab) {
        log_error("Invalid PLT/GOT parameters");
        return;
    }

    size_t relCount = pltRelSize / sizeof(Elf32_Rel);
    log_info("Resolving %zu dynamic symbols...", relCount);

    int resolved = 0;
    int stubbed = 0;

    for (size_t i = 0; i < relCount; ++i) {
        Elf32_Rel* rel = (Elf32_Rel*)(jmpRel + i * sizeof(Elf32_Rel));
        uint32_t symIdx = ELF32_R_SYM(rel->r_info);
        Elf32_Sym* sym = (Elf32_Sym*)(symTab + symIdx * sizeof(Elf32_Sym));
        const char* name = (const char*)(strTab + sym->st_name);
        uintptr_t addr = GetExternalAddr(name);

        if (addr == (uintptr_t)&UnimplementedStub) {
            stubbed++;
        }
        else {
            resolved++;
        }

        DWORD oldProtect;
        VirtualProtect((LPVOID)rel->r_offset, 4, PAGE_READWRITE, &oldProtect);
        *(uintptr_t*)(rel->r_offset) = addr;
        VirtualProtect((LPVOID)rel->r_offset, 4, oldProtect, &oldProtect);
    }

    log_info("Symbol resolution complete: %d resolved, %d stubbed", resolved, stubbed);
}
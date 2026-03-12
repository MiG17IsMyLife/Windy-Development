#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define HAVE_STRUCT_TIMESPEC
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <objbase.h>
#include <excpt.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif
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
#include <string>
#include <math.h>
#include <stdlib.h>
#include <fstream>
#include <set>
#include <crtdbg.h>
#include "../api/graphics/glhooks.h"

// --- OpenGL ---
#ifndef APIENTRY
#define APIENTRY __stdcall
#endif
#include <GL/gl.h>

// --- Pthreads Emulation ---
#include "../api/libc/pthread_emu.h"

#include "symbolresolver.h"
#include "sharedobjectmanager.h"
#include "common.h"
#include "log.h"

// --- Bridges ---
#include "../api/sega/libsegaapi.h"
#include "../api/graphics/x11bridge.h"
#include "../api/graphics/glxbridge.h"
#include "../api/graphics/glutbridge.h"
#include "../api/graphics/glhooks.h"
#include "../api/libc/libcbridge.h"

// --- Linux Bridges ---
#include "../api/linux/linuxtypes.h"
#include "../api/linux/gccbridge.h"
#include "../api/linux/ipcbridge.h"
#include "../api/linux/processbridge.h"
#include "../api/linux/filesystembridge.h"
#include "../api/linux/hardwarebridge.h"

// --- Helper Macros ---
#define ELF32_R_SYM(val) ((val) >> 8)

// --- GPU Spoofing (OpenGL) ---
static const char *GPU_VENDOR_STRING = "NVIDIA Corporation";
static const char *GPU_RENDERER_STRING = "GeForce 7800/PCIe/SSE2";
static const char *GPU_VERSION_STRING = "2.1.2 NVIDIA 285.05.09";
static const char *GPU_SL_VERSION_STRING = "1.20 NVIDIA via Cg compiler";

// =============================================================
//   Stubs & Utilities
// =============================================================

extern "C" int my_stub_success()
{
    return 0;
}
extern "C" int my_stub_fail()
{
    return -1;
}
extern "C" void *my_stub_null()
{
    return NULL;
}

// =============================================================
//   Semaphore Wrappers with Logging (using pthread_emu)
// =============================================================

extern "C" int my_sem_init(void *sem, int pshared, unsigned int value)
{
    log_info(">>> sem_init: sem=%p, pshared=%d, value=%u", sem, pshared, value);
    int ret = emu_sem_init(sem, pshared, value);
    log_info(">>> sem_init EXIT: ret=%d", ret);
    return ret;
}

extern "C" int my_sem_destroy(void *sem)
{
    log_debug(">>> sem_destroy: sem=%p", sem);
    return emu_sem_destroy(sem);
}

extern "C" int my_sem_wait(void *sem)
{
    log_info(">>> sem_wait ENTRY: sem=%p (THIS MAY BLOCK!)", sem);
    int ret = emu_sem_wait(sem);
    log_info(">>> sem_wait EXIT: ret=%d", ret);
    return ret;
}

extern "C" int my_sem_trywait(void *sem)
{
    log_debug(">>> sem_trywait: sem=%p", sem);
    int ret = emu_sem_trywait(sem);
    log_debug(">>> sem_trywait EXIT: ret=%d", ret);
    return ret;
}

extern "C" int my_sem_post(void *sem)
{
    log_debug(">>> sem_post: sem=%p", sem);
    int ret = emu_sem_post(sem);
    return ret;
}

extern "C" int my_sem_getvalue(void *sem, int *sval)
{
    return emu_sem_getvalue(sem, sval);
}

// =============================================================
//   Scheduling Stubs
// =============================================================

extern "C" int my_sched_get_priority_min(int policy)
{
    (void)policy;
    return 0;
}

extern "C" int my_sched_get_priority_max(int policy)
{
    (void)policy;
    return 99;
}

void UnimplementedStub()
{
#ifdef _MSC_VER
    void *ret_addr = _ReturnAddress();
#else
    void *ret_addr = __builtin_return_address(0);
#endif
    log_error("Unimplemented Function Called at %p", ret_addr);
    __debugbreak();
    MessageBoxA(NULL, "Unimplemented Function Called", "Error", MB_OK);
    TerminateProcess(GetCurrentProcess(), 1);
}

int ExceptionFilter(unsigned int code, struct _EXCEPTION_POINTERS *ep)
{
    if (code == EXCEPTION_ACCESS_VIOLATION)
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// Named stub handler — logs which specific symbol was called
extern "C" void __cdecl NamedUnimplementedHandler(const char *symbolName)
{
    log_error("UNIMPLEMENTED SYMBOL CALLED: '%s'", symbolName);
    MessageBoxA(NULL, symbolName, "Unimplemented Function Called", MB_OK);
    TerminateProcess(GetCurrentProcess(), 1);
}

// Set of all named stub thunk addresses for tracking
static std::set<uintptr_t> NamedStubAddresses;

// Check if an address is any kind of stub (generic or named)
static bool IsStub(uintptr_t addr)
{
    if (addr == 0 || addr == (uintptr_t)&UnimplementedStub)
        return true;
    return NamedStubAddresses.count(addr) > 0;
}

// Create a per-symbol stub that calls NamedUnimplementedHandler with the symbol name
static uintptr_t CreateNamedStub(const char *name)
{
    // Allocate persistent copy of the name
    size_t nameLen = strlen(name) + 1;
    char *nameCopy = (char *)VirtualAlloc(NULL, nameLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!nameCopy)
        return (uintptr_t)&UnimplementedStub;
    memcpy(nameCopy, name, nameLen);

    // Allocate executable thunk
    uint8_t *thunk = (uint8_t *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!thunk)
        return (uintptr_t)&UnimplementedStub;

    uint8_t *p = thunk;

    // push imm32 (name pointer as argument)
    *p++ = 0x68;
    *(uint32_t *)p = (uint32_t)nameCopy;
    p += 4;

    // call NamedUnimplementedHandler
    *p++ = 0xE8;
    uint32_t relAddr = (uint32_t)&NamedUnimplementedHandler - ((uint32_t)p + 4);
    *(uint32_t *)p = relAddr;
    p += 4;

    // add esp, 4  (cleanup arg — won't be reached since handler terminates, but correct)
    *p++ = 0x83;
    *p++ = 0xC4;
    *p++ = 0x04;

    // int3 (safety breakpoint)
    *p++ = 0xCC;

    uintptr_t stubAddr = (uintptr_t)thunk;
    NamedStubAddresses.insert(stubAddr);
    return stubAddr;
}

// GPU Spoofing
extern "C" const GLubyte *my_glGetString(GLenum name)
{
    switch (name)
    {
        case 0x1F00:
            return (const GLubyte *)GPU_VENDOR_STRING;
        case 0x1F01:
            return (const GLubyte *)GPU_RENDERER_STRING;
        case 0x1F02:
            return (const GLubyte *)GPU_VERSION_STRING;
        case 0x8B8C:
            return (const GLubyte *)GPU_SL_VERSION_STRING;
    }
    static auto real_glGetString = (const GLubyte *(APIENTRY *)(GLenum))GetProcAddress(LoadLibraryA("opengl32.dll"), "glGetString");
    if (real_glGetString)
        return real_glGetString(name);
    return NULL;
}

// --- Missing Libc / System Functions ---

extern "C" void my_bzero(void *s, size_t n)
{
    memset(s, 0, n);
}

extern "C" int my_bcmp(const void *s1, const void *s2, size_t n)
{
    return memcmp(s1, s2, n);
}

extern "C" int my_clock_gettime(int clk_id, struct timespec *tp)
{
    if (!tp)
        return -1;
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

extern "C" int my_fsync(int fd)
{
    return _commit(fd);
}
extern "C" int my_fdatasync(int fd)
{
    return _commit(fd);
}
extern "C" void my_sync(void)
{
    _flushall();
}

extern "C" int my_access(const char *pathname, int mode)
{
    if (pathname && strstr(pathname, "/dev/") != NULL)
    {
        return 0; // Success
    }
    if (mode == 1)
        mode = 0;
    return _access(pathname, mode);
}

extern "C" int my_chdir(const char *path)
{
    log_debug("chdir: %s", path);
    return _chdir(path);
}

extern "C" int my_chmod(const char *filename, int pmode)
{
    return _chmod(filename, pmode);
}
extern "C" long my_lseek(int fd, long offset, int origin)
{
    return _lseek(fd, offset, origin);
}


extern "C" void my_syslog(int priority, const char *format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log_info("syslog: %s", buffer);
}

// --- Env Wrappers ---

extern "C" int my_setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name))
        return 0;
    return _putenv_s(name, value);
}

extern "C" int my_unsetenv(const char *name)
{
    return _putenv_s(name, "");
}

extern "C" int my_putenv(char *string)
{
    return _putenv(string);
}

// Stub for sysctl always returning -1 (failure/not implemented)
extern "C" int my_sysctl(int *name, int nlen, void *oldval, size_t *oldlenp, void *newval, size_t newlen)
{
    return -1;
}

// --- Select Wrapper ---
// --- Select Wrapper ---
// Handles timeout-only usage (common in loops) or logs usage with FDs

// Macros for Linux FD_SET (matching standard Linux behavior)
#define LINUX_NFDBITS (8 * sizeof(unsigned long))
#define LINUX_FD_SET_BIT(n, p) ((p)->fds_bits[(n) / LINUX_NFDBITS] |= (1UL << ((n) % LINUX_NFDBITS)))
#define LINUX_FD_CLR_BIT(n, p) ((p)->fds_bits[(n) / LINUX_NFDBITS] &= ~(1UL << ((n) % LINUX_NFDBITS)))
#define LINUX_FD_ISSET_BIT(n, p) ((p)->fds_bits[(n) / LINUX_NFDBITS] & (1UL << ((n) % LINUX_NFDBITS)))
#define LINUX_FD_ZERO_BIT(p) memset((void *)(p), 0, sizeof(*(p)))

extern "C" int my_select(int nfds, linux_fd_set *readfds, linux_fd_set *writefds, linux_fd_set *exceptfds, struct timeval *timeout)
{
    // log_info(">>> select called: nfds=%d, timeout=%s", nfds, timeout ? "set" : "NULL");

    if (!readfds && !writefds && !exceptfds && timeout)
    {
        DWORD ms = (timeout->tv_sec * 1000) + (timeout->tv_usec / 1000);
        Sleep(ms);
        return 0;
    }

    fd_set win_read, win_write, win_except;
    FD_ZERO(&win_read);
    FD_ZERO(&win_write);
    FD_ZERO(&win_except);

    fd_set *p_win_read = readfds ? &win_read : NULL;
    fd_set *p_win_write = writefds ? &win_write : NULL;
    fd_set *p_win_except = exceptfds ? &win_except : NULL;

    // Convert Linux sets to Windows sets
    // We iterate from 0 to nfds-1 and check bits
    int safe_nfds = (nfds > LINUX_FD_SETSIZE) ? LINUX_FD_SETSIZE : nfds;

    for (int i = 0; i < safe_nfds; i++)
    {
        if (readfds && LINUX_FD_ISSET_BIT(i, readfds))
        {
            FD_SET((SOCKET)i, &win_read);
        }
        if (writefds && LINUX_FD_ISSET_BIT(i, writefds))
        {
            FD_SET((SOCKET)i, &win_write);
        }
        if (exceptfds && LINUX_FD_ISSET_BIT(i, exceptfds))
        {
            FD_SET((SOCKET)i, &win_except);
        }
    }

    // Call Windows select
    // Note: Windows select ignores nfds, but we pass it anyway (ignored)
    int ret = select(nfds, p_win_read, p_win_write, p_win_except, timeout);

    // If successful (and not just timeout), map results back to Linux sets
    if (ret > 0)
    {
        if (readfds)
            LINUX_FD_ZERO_BIT(readfds);
        if (writefds)
            LINUX_FD_ZERO_BIT(writefds);
        if (exceptfds)
            LINUX_FD_ZERO_BIT(exceptfds);

        for (int i = 0; i < safe_nfds; i++)
        {
            if (p_win_read && FD_ISSET((SOCKET)i, p_win_read))
            {
                LINUX_FD_SET_BIT(i, readfds);
            }
            if (p_win_write && FD_ISSET((SOCKET)i, p_win_write))
            {
                LINUX_FD_SET_BIT(i, writefds);
            }
            if (p_win_except && FD_ISSET((SOCKET)i, p_win_except))
            {
                LINUX_FD_SET_BIT(i, exceptfds);
            }
        }
    }
    else if (ret == 0)
    {
        // Timeout: clear all sets
        if (readfds)
            LINUX_FD_ZERO_BIT(readfds);
        if (writefds)
            LINUX_FD_ZERO_BIT(writefds);
        if (exceptfds)
            LINUX_FD_ZERO_BIT(exceptfds);
    }

    return ret;
}

// =============================================================
//   Poll Wrapper (with logging)
// =============================================================

// Linux pollfd structure
struct linux_pollfd
{
    int fd;
    short events;
    short revents;
};

#define LINUX_POLLIN 0x0001
#define LINUX_POLLOUT 0x0004
#define LINUX_POLLERR 0x0008
#define LINUX_POLLHUP 0x0010
#define LINUX_POLLNVAL 0x0020

extern "C" int my_poll(struct linux_pollfd *fds, unsigned long nfds, int timeout)
{
    // log_info(">>> poll called: nfds=%lu, timeout=%d", nfds, timeout);

    // If no fds, just sleep
    if (nfds == 0)
    {
        if (timeout > 0)
        {
            Sleep(timeout);
        }
        return 0;
    }

    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    int maxfd = 0;
    bool hasValidFd = false;

    for (unsigned long i = 0; i < nfds; i++)
    {
        fds[i].revents = 0;
        if (fds[i].fd < 0)
            continue;

        hasValidFd = true;
        if (fds[i].fd > maxfd)
            maxfd = fds[i].fd;

        if (fds[i].events & LINUX_POLLIN)
        {
            FD_SET((SOCKET)fds[i].fd, &readfds);
        }
        if (fds[i].events & LINUX_POLLOUT)
        {
            FD_SET((SOCKET)fds[i].fd, &writefds);
        }
        // Always monitor for errors
        FD_SET((SOCKET)fds[i].fd, &exceptfds);
    }

    if (!hasValidFd)
    {
        if (timeout > 0)
            Sleep(timeout);
        return 0;
    }

    struct timeval tv;
    struct timeval *tvp = NULL;
    if (timeout >= 0)
    {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        tvp = &tv;
    }

    int ret = select(maxfd + 1, &readfds, &writefds, &exceptfds, tvp);

    if (ret > 0)
    {
        int count = 0;
        for (unsigned long i = 0; i < nfds; i++)
        {
            if (fds[i].fd < 0)
                continue;

            if (FD_ISSET((SOCKET)fds[i].fd, &readfds))
            {
                fds[i].revents |= LINUX_POLLIN;
            }
            if (FD_ISSET((SOCKET)fds[i].fd, &writefds))
            {
                fds[i].revents |= LINUX_POLLOUT;
            }
            if (FD_ISSET((SOCKET)fds[i].fd, &exceptfds))
            {
                fds[i].revents |= LINUX_POLLERR;
            }
            if (fds[i].revents)
                count++;
        }
        return count;
    }
    else if (ret == 0)
    {
        return 0;
    }
    else
    {
        // log_error(">>> poll: select failed");
        return -1;
    }
}

// =============================================================
//   Socket Wrappers (with logging)
// =============================================================

extern "C" SOCKET my_socket(int af, int type, int protocol)
{
    log_info(">>> socket called: af=%d, type=%d, protocol=%d", af, type, protocol);
    SOCKET s = socket(af, type, protocol);
    log_info(">>> socket EXIT: returning %lld", (long long)s);
    return s;
}

extern "C" int my_connect(SOCKET s, const struct sockaddr *name, int namelen)
{
    log_info(">>> connect called: socket=%lld", (long long)s);
    int ret = connect(s, name, namelen);
    log_info(">>> connect EXIT: returning %d (WSAError=%d)", ret, ret < 0 ? WSAGetLastError() : 0);
    return ret;
}

extern "C" int my_bind(SOCKET s, const struct sockaddr *name, int namelen)
{
    log_info(">>> bind called: socket=%lld", (long long)s);
    int ret = bind(s, name, namelen);
    log_info(">>> bind EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_listen(SOCKET s, int backlog)
{
    log_info(">>> listen called: socket=%lld, backlog=%d", (long long)s, backlog);
    int ret = listen(s, backlog);
    log_info(">>> listen EXIT: returning %d", ret);
    return ret;
}

extern "C" SOCKET my_accept(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    log_info(">>> accept ENTRY: socket=%lld (THIS MAY BLOCK!)", (long long)s);
    SOCKET ret = accept(s, addr, addrlen);
    log_info(">>> accept EXIT: returning %lld (WSAError=%d)", (long long)ret, ret == INVALID_SOCKET ? WSAGetLastError() : 0);
    return ret;
}

extern "C" int my_recv(SOCKET s, char *buf, int len, int flags)
{
    log_info(">>> recv ENTRY: socket=%lld, len=%d (THIS MAY BLOCK!)", (long long)s, len);
    int ret = recv(s, buf, len, flags);
    log_info(">>> recv EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_send(SOCKET s, const char *buf, int len, int flags)
{
    log_info(">>> send called: socket=%lld, len=%d", (long long)s, len);
    int ret = send(s, buf, len, flags);
    log_info(">>> send EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
    log_info(">>> recvfrom ENTRY: socket=%lld, len=%d (THIS MAY BLOCK!)", (long long)s, len);
    int ret = recvfrom(s, buf, len, flags, from, fromlen);
    log_info(">>> recvfrom EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
    log_info(">>> sendto called: socket=%lld, len=%d", (long long)s, len);
    int ret = sendto(s, buf, len, flags, to, tolen);
    log_info(">>> sendto EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_setsockopt(SOCKET s, int level, int optname, const char *optval, int optlen)
{
    log_info(">>> setsockopt called: socket=%lld, level=%d, optname=%d", (long long)s, level, optname);
    int ret = setsockopt(s, level, optname, optval, optlen);
    log_info(">>> setsockopt EXIT: returning %d", ret);
    return ret;
}

extern "C" int my_getsockopt(SOCKET s, int level, int optname, char *optval, int *optlen)
{
    log_info(">>> getsockopt called: socket=%lld, level=%d, optname=%d", (long long)s, level, optname);
    int ret = getsockopt(s, level, optname, optval, optlen);
    log_info(">>> getsockopt EXIT: returning %d", ret);
    return ret;
}

// --- Lindbergh Specific Stubs ---
void MyInvalidParameterHandler(const wchar_t *expression, const wchar_t *function, const wchar_t *file, unsigned int line,
                               uintptr_t pReserved)
{
    printf("CRT Invalid Parameter detected in function: %ws\n", function ? function : L"Unknown");
}

extern "C" int my_kswap_collect(void *p)
{
    return 0;
}

// Entrypoint hooking
typedef int (*MainFunc)(int, char **, char **);

extern "C" void my_libc_start_main(MainFunc m, int c, char **a, void (*i)(), void (*f)(), void (*r)(), void *s)
{
    // handler for invailed parameter
    _set_invalid_parameter_handler(MyInvalidParameterHandler);

    _CrtSetReportMode(_CRT_ASSERT, 0);
    _CrtSetReportMode(_CRT_ERROR, 0);

    log_info("__libc_start_main called");

    // Initialize pthread emulation
    PthreadEmu::Initialize();
    log_info("PthreadEmu initialized");

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (i)
    {
        log_debug("Calling init function");
#ifdef _MSC_VER
        __try
        {
            i();
        }
        __except (ExceptionFilter(GetExceptionCode(), GetExceptionInformation()))
        {
            log_error("Exception in init function");
        }
#else
        i();
#endif
    }
    int result = 0;
    log_info("Calling main(argc=%d)", c);

    static char *empty_env[] = {NULL};

#ifdef _MSC_VER
    __try
    {

        result = m(c, a, empty_env);
    }
    __except (ExceptionFilter(GetExceptionCode(), GetExceptionInformation()))
    {
        log_fatal("Exception in main() - process terminated");
        exit(1);
    }
#else
    result = m(c, a, empty_env);
#endif
    log_info("main() returned %d", result);

    // Cleanup pthread emulation
    PthreadEmu::Shutdown();
    log_info("PthreadEmu shutdown complete");

    exit(result);
}

// =============================================================
//   C++ Exception Handling Stub
// =============================================================
extern "C" int my_gxx_personality_v0(int version, int actions, uint64_t exceptionClass, void *exceptionObject, void *context)
{
    (void)version;
    (void)actions;
    (void)exceptionClass;
    (void)exceptionObject;
    (void)context;
    log_error("__gxx_personality_v0 called! C++ Exceptions not supported.");
    return 9; // _URC_FATAL_PHASE1_ERROR
}

// extern "C" void my_cfmakeraw(void *termios_p)
//{
//     (void)termios_p;
//     log_debug("cfmakeraw called - stubbed");
// }

extern "C" void my_operator_delete(void *ptr)
{
    log_debug("operator delete(_ZdlPv) called: %p", ptr);
    free(ptr);
}

extern "C" void *my_operator_new(size_t size)
{
    log_debug("operator new(_Znwj) called: size=%zu", size);
    return malloc(size);
}

extern "C" void *my_operator_new_array(size_t size)
{
    log_debug("operator new[](_Znaj) called: size=%zu", size);
    return malloc(size);
}

extern "C" void __cdecl LogSymbolCall(const char *name, uintptr_t caller)
{
    FILE *f = fopen("symbol_trace.log", "a");
    if (f)
    {
        fprintf(f, "[SYMCALL] %s called from 0x%08X\n", name, (unsigned int)caller);
        fclose(f);
    }
    // printf("[SYMCALL] %s called from 0x%08X\n", name, (unsigned int)caller);
}

uintptr_t CreateThunk(const char *name, uintptr_t target)
{

    uint8_t *thunk = (uint8_t *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!thunk)
        return target;

    uint8_t *p = thunk;

    *p++ = 0x50; // push eax
    *p++ = 0x51; // push ecx
    *p++ = 0x52; // push edx

    *p++ = 0x8B;
    *p++ = 0x44;
    *p++ = 0x24;
    *p++ = 0x0C;

    *p++ = 0x50; // push eax (Arg2: caller address)

    *p++ = 0x68; // push imm32 (Arg1: name pointer)
    *(uint32_t *)p = (uint32_t)name;
    p += 4;

    *p++ = 0xE8; // call rel32
    uint32_t relAddr = (uint32_t)&LogSymbolCall - ((uint32_t)p + 4);
    *(uint32_t *)p = relAddr;
    p += 4;

    //  Cleanup arguments
    *p++ = 0x83;
    *p++ = 0xC4;
    *p++ = 0x08;

    *p++ = 0x5A; // pop edx
    *p++ = 0x59; // pop ecx
    *p++ = 0x58; // pop eax

    *p++ = 0xE9; // jmp rel32
    relAddr = (uint32_t)target - ((uint32_t)p + 4);
    *(uint32_t *)p = relAddr;
    p += 4;

    return (uintptr_t)thunk;
}

// =============================================================
//   Symbol Resolver Main Function
// =============================================================
// ============================================================
// C++ ABI Stubs (Itanium / GCC 3.x)
// ============================================================

// __gnu_cxx::__exchange_and_add(volatile int*, int)
static int my_exchange_and_add(volatile int* mem, int val)
{
#if defined(_MSC_VER) || defined(__MINGW32__)
    return InterlockedExchangeAdd((LONG volatile *)mem, val);
#else
    return __sync_fetch_and_add(mem, val);
#endif
}

// std::__default_alloc_template<true, 0>::allocate(unsigned int)
static void *my_SGI_Alloc_Allocate(unsigned int n)
{
    (void)n;
    // return ::operator new(n);
    return malloc(n);
}

// std::__default_alloc_template<true, 0>::deallocate(void*, unsigned int)
static void my_SGI_Alloc_Deallocate(void *p, unsigned int n)
{
    (void)n;
    // ::operator delete(p);
    free(p);
}

// --- std::_Rb_tree node base ---
struct GCC_Rb_tree_node_base
{
    int _M_color; // 0 = red, 1 = black
    GCC_Rb_tree_node_base* _M_parent;
    GCC_Rb_tree_node_base* _M_left;
    GCC_Rb_tree_node_base* _M_right;
};

// void std::_Rb_tree_insert_and_rebalance(bool insert_left, _Rb_tree_node_base* x, _Rb_tree_node_base* p, _Rb_tree_node_base& header)
static void my_Rb_tree_insert_and_rebalance(bool insert_left, void* x_ptr, void* p_ptr, void* header_ptr)
{
    GCC_Rb_tree_node_base* x = (GCC_Rb_tree_node_base*)x_ptr;
    GCC_Rb_tree_node_base* p = (GCC_Rb_tree_node_base*)p_ptr;
    GCC_Rb_tree_node_base* header = (GCC_Rb_tree_node_base*)header_ptr;

    x->_M_parent = p;
    x->_M_left = 0;
    x->_M_right = 0;
    x->_M_color = 0; // red

    if (insert_left) {
        p->_M_left = x; // also makes leftmost = x when p == header
        if (p == header) {
            header->_M_parent = x;
            header->_M_right = x;
        } else if (p == header->_M_left) {
            header->_M_left = x; // maintain leftmost pointing to min node
        }
    } else {
        p->_M_right = x;
        if (p == header->_M_right) {
            header->_M_right = x; // maintain rightmost pointing to max node
        }
    }

    // rebalance
    while (x != header->_M_parent && x->_M_parent->_M_color == 0) {
        GCC_Rb_tree_node_base* xpp = x->_M_parent->_M_parent;
        if (x->_M_parent == xpp->_M_left) {
            GCC_Rb_tree_node_base* y = xpp->_M_right;
            if (y && y->_M_color == 0) {
                x->_M_parent->_M_color = 1;
                y->_M_color = 1;
                xpp->_M_color = 0;
                x = xpp;
            } else {
                if (x == x->_M_parent->_M_right) {
                    x = x->_M_parent;
                    GCC_Rb_tree_node_base* y_left = x->_M_right;
                    x->_M_right = y_left->_M_left;
                    if (y_left->_M_left != 0) y_left->_M_left->_M_parent = x;
                    y_left->_M_parent = x->_M_parent;
                    if (x == header->_M_parent) header->_M_parent = y_left;
                    else if (x == x->_M_parent->_M_left) x->_M_parent->_M_left = y_left;
                    else x->_M_parent->_M_right = y_left;
                    y_left->_M_left = x;
                    x->_M_parent = y_left;
                }
                x->_M_parent->_M_color = 1;
                xpp->_M_color = 0;
                GCC_Rb_tree_node_base* y_left = xpp->_M_left;
                xpp->_M_left = y_left->_M_right;
                if (y_left->_M_right != 0) y_left->_M_right->_M_parent = xpp;
                y_left->_M_parent = xpp->_M_parent;
                if (xpp == header->_M_parent) header->_M_parent = y_left;
                else if (xpp == xpp->_M_parent->_M_right) xpp->_M_parent->_M_right = y_left;
                else xpp->_M_parent->_M_left = y_left;
                y_left->_M_right = xpp;
                xpp->_M_parent = y_left;
            }
        } else {
            GCC_Rb_tree_node_base* y = xpp->_M_left;
            if (y && y->_M_color == 0) {
                x->_M_parent->_M_color = 1;
                y->_M_color = 1;
                xpp->_M_color = 0;
                x = xpp;
            } else {
                if (x == x->_M_parent->_M_left) {
                    x = x->_M_parent;
                    GCC_Rb_tree_node_base* y_left = x->_M_left;
                    x->_M_left = y_left->_M_right;
                    if (y_left->_M_right != 0) y_left->_M_right->_M_parent = x;
                    y_left->_M_parent = x->_M_parent;
                    if (x == header->_M_parent) header->_M_parent = y_left;
                    else if (x == x->_M_parent->_M_right) x->_M_parent->_M_right = y_left;
                    else x->_M_parent->_M_left = y_left;
                    y_left->_M_right = x;
                    x->_M_parent = y_left;
                }
                x->_M_parent->_M_color = 1;
                xpp->_M_color = 0;
                GCC_Rb_tree_node_base* y_left = xpp->_M_right;
                xpp->_M_right = y_left->_M_left;
                if (y_left->_M_left != 0) y_left->_M_left->_M_parent = xpp;
                y_left->_M_parent = xpp->_M_parent;
                if (xpp == header->_M_parent) header->_M_parent = y_left;
                else if (xpp == xpp->_M_parent->_M_left) xpp->_M_parent->_M_left = y_left;
                else xpp->_M_parent->_M_right = y_left;
                y_left->_M_left = xpp;
                xpp->_M_parent = y_left;
            }
        }
    }
    header->_M_parent->_M_color = 1; // root is always black
}

// _Rb_tree_node_base* std::_Rb_tree_rebalance_for_erase(_Rb_tree_node_base* z, _Rb_tree_node_base& header)
static void* my_Rb_tree_rebalance_for_erase(void* z_ptr, void* header_ptr)
{
    GCC_Rb_tree_node_base* z = (GCC_Rb_tree_node_base*)z_ptr;
    GCC_Rb_tree_node_base* header = (GCC_Rb_tree_node_base*)header_ptr;
    GCC_Rb_tree_node_base* y = z;
    GCC_Rb_tree_node_base* x = 0;
    GCC_Rb_tree_node_base* x_parent = 0;

    if (y->_M_left == 0) {
        x = y->_M_right;
    } else if (y->_M_right == 0) {
        x = y->_M_left;
    } else {
        y = y->_M_right;
        while (y->_M_left != 0) y = y->_M_left;
        x = y->_M_right;
    }

    if (y != z) {
        z->_M_left->_M_parent = y;
        y->_M_left = z->_M_left;
        if (y != z->_M_right) {
            x_parent = y->_M_parent;
            if (x) x->_M_parent = y->_M_parent;
            y->_M_parent->_M_left = x;
            y->_M_right = z->_M_right;
            z->_M_right->_M_parent = y;
        } else {
            x_parent = y;
        }
        if (header->_M_parent == z) header->_M_parent = y;
        else if (z->_M_parent->_M_left == z) z->_M_parent->_M_left = y;
        else z->_M_parent->_M_right = y;
        y->_M_parent = z->_M_parent;
        int tmp_color = y->_M_color;
        y->_M_color = z->_M_color;
        z->_M_color = tmp_color;
        y = z;
    } else {
        x_parent = y->_M_parent;
        if (x) x->_M_parent = y->_M_parent;
        if (header->_M_parent == z) header->_M_parent = x;
        else if (z->_M_parent->_M_left == z) z->_M_parent->_M_left = x;
        else z->_M_parent->_M_right = x;
        if (header->_M_left == z) {
            if (z->_M_right == 0) header->_M_left = z->_M_parent;
            else header->_M_left = x; // min(x)
        }
        if (header->_M_right == z) {
            if (z->_M_left == 0) header->_M_right = z->_M_parent;
            else {
                GCC_Rb_tree_node_base* t = x;
                while (t->_M_right) t = t->_M_right;
                header->_M_right = t;
            }
        }
    }

    if (y->_M_color != 0) {
        while (x != header->_M_parent && (x == 0 || x->_M_color == 1)) {
            if (x == x_parent->_M_left) {
                GCC_Rb_tree_node_base* w = x_parent->_M_right;
                if (w->_M_color == 0) {
                    w->_M_color = 1;
                    x_parent->_M_color = 0;
                    GCC_Rb_tree_node_base* t = x_parent->_M_right;
                    x_parent->_M_right = t->_M_left;
                    if (t->_M_left) t->_M_left->_M_parent = x_parent;
                    t->_M_parent = x_parent->_M_parent;
                    if (x_parent == header->_M_parent) header->_M_parent = t;
                    else if (x_parent == x_parent->_M_parent->_M_left) x_parent->_M_parent->_M_left = t;
                    else x_parent->_M_parent->_M_right = t;
                    t->_M_left = x_parent;
                    x_parent->_M_parent = t;
                    w = x_parent->_M_right;
                }
                if ((w->_M_left == 0 || w->_M_left->_M_color == 1) &&
                    (w->_M_right == 0 || w->_M_right->_M_color == 1)) {
                    w->_M_color = 0;
                    x = x_parent;
                    x_parent = x_parent->_M_parent;
                } else {
                    if (w->_M_right == 0 || w->_M_right->_M_color == 1) {
                        if (w->_M_left) w->_M_left->_M_color = 1;
                        w->_M_color = 0;
                        GCC_Rb_tree_node_base* t = w->_M_left;
                        w->_M_left = t->_M_right;
                        if (t->_M_right) t->_M_right->_M_parent = w;
                        t->_M_parent = w->_M_parent;
                        if (w == header->_M_parent) header->_M_parent = t;
                        else if (w == w->_M_parent->_M_right) w->_M_parent->_M_right = t;
                        else w->_M_parent->_M_left = t;
                        t->_M_right = w;
                        w->_M_parent = t;
                        w = x_parent->_M_right;
                    }
                    w->_M_color = x_parent->_M_color;
                    x_parent->_M_color = 1;
                    if (w->_M_right) w->_M_right->_M_color = 1;
                    GCC_Rb_tree_node_base* t = x_parent->_M_right;
                    x_parent->_M_right = t->_M_left;
                    if (t->_M_left) t->_M_left->_M_parent = x_parent;
                    t->_M_parent = x_parent->_M_parent;
                    if (x_parent == header->_M_parent) header->_M_parent = t;
                    else if (x_parent == x_parent->_M_parent->_M_left) x_parent->_M_parent->_M_left = t;
                    else x_parent->_M_parent->_M_right = t;
                    t->_M_left = x_parent;
                    x_parent->_M_parent = t;
                    break;
                }
            } else {
                GCC_Rb_tree_node_base* w = x_parent->_M_left;
                if (w->_M_color == 0) {
                    w->_M_color = 1;
                    x_parent->_M_color = 0;
                    GCC_Rb_tree_node_base* t = x_parent->_M_left;
                    x_parent->_M_left = t->_M_right;
                    if (t->_M_right) t->_M_right->_M_parent = x_parent;
                    t->_M_parent = x_parent->_M_parent;
                    if (x_parent == header->_M_parent) header->_M_parent = t;
                    else if (x_parent == x_parent->_M_parent->_M_right) x_parent->_M_parent->_M_right = t;
                    else x_parent->_M_parent->_M_left = t;
                    t->_M_right = x_parent;
                    x_parent->_M_parent = t;
                    w = x_parent->_M_left;
                }
                if ((w->_M_right == 0 || w->_M_right->_M_color == 1) &&
                    (w->_M_left == 0 || w->_M_left->_M_color == 1)) {
                    w->_M_color = 0;
                    x = x_parent;
                    x_parent = x_parent->_M_parent;
                } else {
                    if (w->_M_left == 0 || w->_M_left->_M_color == 1) {
                        if (w->_M_right) w->_M_right->_M_color = 1;
                        w->_M_color = 0;
                        GCC_Rb_tree_node_base* t = w->_M_right;
                        w->_M_right = t->_M_left;
                        if (t->_M_left) t->_M_left->_M_parent = w;
                        t->_M_parent = w->_M_parent;
                        if (w == header->_M_parent) header->_M_parent = t;
                        else if (w == w->_M_parent->_M_left) w->_M_parent->_M_left = t;
                        else w->_M_parent->_M_right = t;
                        t->_M_left = w;
                        w->_M_parent = t;
                        w = x_parent->_M_left;
                    }
                    w->_M_color = x_parent->_M_color;
                    x_parent->_M_color = 1;
                    if (w->_M_left) w->_M_left->_M_color = 1;
                    GCC_Rb_tree_node_base* t = x_parent->_M_left;
                    x_parent->_M_left = t->_M_right;
                    if (t->_M_right) t->_M_right->_M_parent = x_parent;
                    t->_M_parent = x_parent->_M_parent;
                    if (x_parent == header->_M_parent) header->_M_parent = t;
                    else if (x_parent == x_parent->_M_parent->_M_right) x_parent->_M_parent->_M_right = t;
                    else x_parent->_M_parent->_M_left = t;
                    t->_M_right = x_parent;
                    x_parent->_M_parent = t;
                    break;
                }
            }
        }
        if (x) x->_M_color = 1;
    }
    return y;
}

// std::ios_base::Init::Init()
static void my_IosBaseInit_Ctor(void *thisptr)
{
    new (thisptr) std::ios_base::Init();
}

// std::ios_base::Init::~Init()
static void my_IosBaseInit_Dtor(void *thisptr)
{
    ((std::ios_base::Init *)thisptr)->~Init();
}

struct MyFakeIosBase : public std::ios_base {
    MyFakeIosBase() : std::ios_base() {}
};

// std::ios_base::ios_base()
static void my_IosBase_Ctor(void *thisptr)
{
    // Uses a dummy derived class because std::ios_base constructor is protected
    new (thisptr) MyFakeIosBase();
}

// --- std::string (GCC 3.x ABI: _ZNSs...) ---

// GCC 3.x basic_string ABI structures
struct GCC_String_Rep
{
    size_t length;
    size_t capacity;
    int _ref;
    // The actual string data follows in memory immediately after this struct
};

// The actual std::string class on the stack is just a pointer
struct GCC_String
{
    char *_M_dataplus; // Points to the string data (which is prepended by Rep)
};

// std::string::string(char const*, std::allocator<char> const&)
static void my_StdString_ConstChar_Ctor(void *this_ptr, const char *str, void *alloc)
{
    (void)alloc;
    if (!this_ptr || !str)
        return;

    size_t len = strlen(str);

    // Allocate space for Rep header + string data + null terminator
    size_t total_size = sizeof(GCC_String_Rep) + len + 1;
    void *memory = malloc(total_size);

    if (memory)
    {
        // Setup the Rep header
        GCC_String_Rep *rep = (GCC_String_Rep *)memory;
        rep->length = len;
        rep->capacity = len;
        rep->_ref = 0; // Owned by one

        // Copy string data after the header
        char *data_ptr = (char *)(rep + 1);
        strcpy(data_ptr, str);

        // The std::string object itself is just a pointer to the data
        GCC_String *gcc_str = (GCC_String *)this_ptr;
        gcc_str->_M_dataplus = data_ptr;

        log_info("Safe GCC String Constructed: '%s' (Rep: %p, Data: %p)", str, rep, data_ptr);
    }
}

// std::string::~string()
static void my_StdString_Dtor(void *this_ptr)
{
    if (!this_ptr) return;
    GCC_String *this_str = (GCC_String *)this_ptr;
    char *data = this_str->_M_dataplus;
    if (data) {
        GCC_String_Rep *rep = (GCC_String_Rep *)(data - sizeof(GCC_String_Rep));
        if (rep->capacity > 0) {
            rep->_ref--;
            if (rep->_ref < 0) {
                free(rep);
            }
        }
    }
}

// std::string::string()
static void my_StdString_DefaultCtor(void *this_ptr, void *alloc)
{
    (void)alloc;
    if (!this_ptr)
        return;
        
    size_t len = 0;
    size_t total_size = sizeof(GCC_String_Rep) + len + 1;
    void *memory = malloc(total_size);
    if (memory)
    {
        GCC_String_Rep *rep = (GCC_String_Rep *)memory;
        rep->length = len;
        rep->capacity = len;
        rep->_ref = 0;
        
        char *data_ptr = (char *)(rep + 1);
        data_ptr[0] = '\0';
        
        GCC_String *gcc_str = (GCC_String *)this_ptr;
        gcc_str->_M_dataplus = data_ptr;
    }
}

// std::string::string(std::string const&)
// std::string::string(std::string const&)
static void my_StdString_CopyCtor(void *this_ptr, void *other_ptr)
{
    if (!this_ptr || !other_ptr)
        return;

    // other_ptr is pointer to GCC_String (which has _M_dataplus)
    GCC_String *other_str = (GCC_String *)other_ptr;
    char *other_data = other_str->_M_dataplus;

    if (other_data)
    {
        // Retrieve Rep from other string (pointers back -0xC)
        GCC_String_Rep *other_rep = (GCC_String_Rep *)(other_data - sizeof(GCC_String_Rep));

        // Deep Copy allocation
        size_t len = other_rep->length;
        size_t total_size = sizeof(GCC_String_Rep) + len + 1;
        void *memory = malloc(total_size);

        if (memory)
        {
            GCC_String_Rep *rep = (GCC_String_Rep *)memory;
            rep->length = len;
            rep->capacity = len;
            rep->_ref = 0; // Owned by one

            char *data_ptr = (char *)(rep + 1);
            strcpy(data_ptr, other_data);

            GCC_String *this_str = (GCC_String *)this_ptr;
            this_str->_M_dataplus = data_ptr;
            log_debug("GCC String Copy Ctor: '%s'", data_ptr);
        }
    }
}

// std::string::~string() (The implementation often calls _M_destroy)
// _ZNSs4_Rep10_M_destroyERKSaIcE
static void my_StringRep_Destroy(void *rep_ptr, void *alloc)
{
    (void)alloc;
    if (rep_ptr)
    {
        // log_debug("GCC String Rep Destroy: %p", rep_ptr);
        free(rep_ptr);
    }
}

// std::string::_M_mutate(unsigned int, unsigned int, unsigned int)
static void my_StdString_Mutate(void *this_ptr, unsigned int pos, unsigned int len1, unsigned int len2)
{
    if (!this_ptr) return;

    GCC_String *this_str = (GCC_String *)this_ptr;
    char *old_data = this_str->_M_dataplus;
    if (!old_data) return;

    GCC_String_Rep *old_rep = (GCC_String_Rep *)(old_data - sizeof(GCC_String_Rep));
    
    size_t old_length = old_rep->length;
    if (pos > old_length) pos = old_length;
    if (pos + len1 > old_length) len1 = old_length - pos;
    
    size_t new_length = old_length - len1 + len2;
    
    if (old_rep->_ref > 0 || new_length > old_rep->capacity)
    {
        size_t new_capacity = new_length;
        if (new_capacity < 2 * old_rep->capacity) new_capacity = 2 * old_rep->capacity;
        if (new_capacity < 15) new_capacity = 15;
        
        size_t total_size = sizeof(GCC_String_Rep) + new_capacity + 1;
        void *memory = malloc(total_size);
        if (memory)
        {
            GCC_String_Rep *new_rep = (GCC_String_Rep *)memory;
            new_rep->length = new_length;
            new_rep->capacity = new_capacity;
            new_rep->_ref = 0;
            
            char *new_data = (char *)(new_rep + 1);
            
            if (pos > 0)
                memcpy(new_data, old_data, pos);
                
            size_t how_much = old_length - pos - len1;
            if (how_much > 0)
                memcpy(new_data + pos + len2, old_data + pos + len1, how_much);
                
            new_data[new_length] = '\0';
            
            if (old_rep->capacity > 0) {
                old_rep->_ref--;
                if (old_rep->_ref < 0) {
                    free(old_rep);
                }
            }
            
            this_str->_M_dataplus = new_data;
        }
    }
    else
    {
        size_t how_much = old_length - pos - len1;
        if (how_much > 0 && len1 != len2)
        {
            memmove(old_data + pos + len2, old_data + pos + len1, how_much);
        }
        old_rep->length = new_length;
        old_data[new_length] = '\0';
    }
}

// std::string::append(char const*, unsigned int)
static void *my_StdString_Append(void *this_ptr, const char *s, unsigned int n)
{
    if (!this_ptr) return this_ptr;
    
    GCC_String *this_str = (GCC_String *)this_ptr;
    char *data = this_str->_M_dataplus;
    size_t old_length = 0;
    if (data)
    {
        GCC_String_Rep *rep = (GCC_String_Rep *)(data - sizeof(GCC_String_Rep));
        old_length = rep->length;
    }
    
    my_StdString_Mutate(this_ptr, old_length, 0, n);
    
    data = this_str->_M_dataplus;
    if (data && s && n > 0)
    {
        memcpy(data + old_length, s, n);
    }
    
    return this_ptr;
}

// std::string::replace(unsigned int, unsigned int, char const*, unsigned int)
static void *my_StdString_Replace(void *this_ptr, unsigned int pos, unsigned int n1, const char *s, unsigned int n2)
{
    if (!this_ptr) return this_ptr;
    
    my_StdString_Mutate(this_ptr, pos, n1, n2);
    
    GCC_String *this_str = (GCC_String *)this_ptr;
    char *data = this_str->_M_dataplus;
    if (data && s && n2 > 0)
    {
        memcpy(data + pos, s, n2);
    }
    
    return this_ptr;
}

// std::string::assign(std::string const&)
static void *my_StdString_AssignCopy(void *this_ptr, void *other_ptr)
{
    if (!this_ptr || !other_ptr)
        return this_ptr;

    GCC_String *this_str = (GCC_String *)this_ptr;
    GCC_String *other_str = (GCC_String *)other_ptr;

    if (this_str == other_str)
        return this_ptr;

    char *other_data = other_str->_M_dataplus;
    if (other_data)
    {
        GCC_String_Rep *other_rep = (GCC_String_Rep *)(other_data - sizeof(GCC_String_Rep));
        
        size_t len = other_rep->length;
        size_t total_size = sizeof(GCC_String_Rep) + len + 1;
        void *memory = malloc(total_size);

        if (memory)
        {
            GCC_String_Rep *rep = (GCC_String_Rep *)memory;
            rep->length = len;
            rep->capacity = len;
            rep->_ref = 0; // Owned by one

            char *data_ptr = (char *)(rep + 1);
            strcpy(data_ptr, other_data);

            char *old_data = this_str->_M_dataplus;
            if (old_data)
            {
                GCC_String_Rep *old_rep = (GCC_String_Rep *)(old_data - sizeof(GCC_String_Rep));
                // Only attempt to free if it's not the empty rep (capacity > 0)
                if (old_rep->capacity > 0) {
                    old_rep->_ref--;
                    if (old_rep->_ref < 0) {
                        free(old_rep);
                    }
                }
            }

            this_str->_M_dataplus = data_ptr;
            log_debug("GCC String Assign Copy: '%s'", data_ptr);
        }
    }
    return this_ptr;
}

// std::string::assign(char const*, unsigned int)
static void *my_StdString_AssignCharConst(void *this_ptr, const char *s, unsigned int n)
{
    if (!this_ptr) return this_ptr;
    
    GCC_String *this_str = (GCC_String *)this_ptr;
    char *old_data = this_str->_M_dataplus;
    size_t old_length = 0;
    
    if (old_data) {
        GCC_String_Rep *old_rep = (GCC_String_Rep *)(old_data - sizeof(GCC_String_Rep));
        old_length = old_rep->length;
    }
    
    // Use replace: replace the entire old string with the new string
    my_StdString_Replace(this_ptr, 0, old_length, s, n);
    return this_ptr;
}

// std::operator<<(std::ostream&, std::string const&)
// _ZStlsIcSt11char_traitsIcESaIcEERSt13basic_ostreamIT_T0_ES7_RSbIS4_S5_T1_E
static void* my_StdString_Ostream_LShift(void* os, void* str_ptr)
{
    if (!os || !str_ptr) return os;
    
    GCC_String* str = (GCC_String*)str_ptr;
    char* data = str->_M_dataplus;
    
    if (data) {
        GCC_String_Rep* rep = (GCC_String_Rep*)(data - sizeof(GCC_String_Rep));
        log_info("STUB: std::ostream << std::string(\"%.*s\")", (int)rep->length, data);
    }
    return os;
}

// std::operator>>(std::istream&, std::string&)
// _ZStrsIcSt11char_traitsIcESaIcEERSt13basic_istreamIT_T0_ES7_RSbIS4_S5_T1_E
static void* my_StdString_Istream_RShift(void* is, void* str_ptr)
{
    (void)str_ptr;
    log_warn("STUB: std::istream >> std::string");
    return is;
}

// std::string::compare(char const*) const
static int my_StdString_CompareConstChar(void *this_ptr, const char *s)
{
    if (!this_ptr || !s) return 0;
    
    GCC_String *this_str = (GCC_String *)this_ptr;
    char *data = this_str->_M_dataplus;
    if (!data) return strcmp("", s);
    
    GCC_String_Rep *rep = (GCC_String_Rep *)(data - sizeof(GCC_String_Rep));
    size_t lhs_len = rep->length;
    size_t rhs_len = strlen(s);
    
    size_t len = lhs_len < rhs_len ? lhs_len : rhs_len;
    int cmp = memcmp(data, s, len);
    if (cmp != 0) return cmp;
    if (lhs_len < rhs_len) return -1;
    if (lhs_len > rhs_len) return 1;
    return 0;
}

// --- Fake Locale Structure ---

struct FakeLocaleStruct
{
    void *localeData[13]; // __locale_data* per LC_* category
    const unsigned short *ctypeB;
    const int *ctypeTolower;
    const int *ctypeToUpper;
    const char *names[13];
};

static FakeLocaleStruct g_DefaultLocale = {};

static void *GetDefaultLocale()
{
    static bool initialized = false;
    if (!initialized)
    {
        memset(&g_DefaultLocale, 0, sizeof(g_DefaultLocale));
        initialized = true;
    }
    return &g_DefaultLocale;
}

// --- Core Locale Functions ---
extern "C" void *my_newlocale(int categoryMask, const char *locale, void *base)
{
    (void)categoryMask;
    (void)locale;
    (void)base;
    log_debug("newlocale(mask=0x%X, locale='%s') -> stub", categoryMask, locale ? locale : "(null)");
    return GetDefaultLocale();
}

extern "C" void my_freelocale(void *locobj)
{
    (void)locobj;
}
extern "C" void *my_uselocale(void *newloc)
{
    (void)newloc;
    return GetDefaultLocale();
}
extern "C" void *my_duplocale(void *locobj)
{
    (void)locobj;
    return GetDefaultLocale();
}

// --- Vector construction/destruction ---

// void *my_cxa_vec_ctor(void *array_address, size_t element_count, size_t element_size, void (*constructor)(void *),
//                       void (*destructor)(void *))
//{
//     size_t i;
//     char *ptr;
//
//     (void)destructor;
//
//     if (element_count == 0)
//         return array_address;
//
//     ptr = (char *)array_address;
//
//     for (i = 0; i < element_count; i++)
//     {
//         if (constructor)
//         {
//             // In a proper implementation, we'd handle exceptions here
//             // by catching, calling destructors on already-constructed elements, and rethrowing.
//             // Since this environment may lack C++ exceptions or unwinding support,
//             // we simplify and assume success.
//             constructor(ptr);
//         }
//         ptr += element_size;
//     }
//     return array_address;
// }

// void my_cxa_vec_dtor(void *array_address, size_t element_count, size_t element_size, void (*destructor)(void *))
//{
//     size_t i;
//     char *ptr;
//
//     if (element_count == 0 || !destructor)
//         return;
//
//     // Destruct in reverse order
//     ptr = (char *)array_address + (element_count - 1) * element_size;
//
//     for (i = 0; i < element_count; i++)
//     {
//         destructor(ptr);
//         ptr -= element_size;
//     }
// }

extern "C" long my_syscall(long number, ...)
{
    log_warn("syscall(%ld) called — returning -ENOSYS", number);
    return -38; // -ENOSYS
}

extern "C" void my___register_frame_info_bases(void *begin, void *ob, void *tbase, void *dbase)
{
}

uintptr_t SymbolResolver::GetExternalAddr(const char *name)
{

    static const std::set<std::string> weakSymbols = {
        "__gmon_start__", "_ITM_deregisterTMCloneTable", "_ITM_registerTMCloneTable", "_Jv_RegisterClasses",
        "__cxa_finalize" // Often weak
    };

    if (weakSymbols.count(name))
    {
        log_info("Resolved weak symbol '%s' to NULL (Not Implemented/Required)", name);
        return 0;
    }

    // Log symbol resolution requests for debugging (only unresolved ones are logged as warnings)
    // log_trace("Resolving symbol: %s", name);

    // 1. Check loaded Linux Shared Objects first
    // This allows functions in libssl.so to find themselves or others
    uintptr_t globalAddr = SharedObjectManager::Instance().GetGlobalSymbol(name);
    if (globalAddr != 0)
        return globalAddr;

    // Standard Mapping Macro
#define MAP(n, f)                                                                                                                          \
    if (strcmp(name, n) == 0)                                                                                                              \
    return (uintptr_t)&f
    // Overloaded Function Mapping Macro (No & operator)
#define MAP_OL(n, f)                                                                                                                       \
    if (strcmp(name, n) == 0)                                                                                                              \
    return (uintptr_t)(f)

    // ============================================================
    // GLUT / GLU
    // ============================================================
    MAP("glutInit", GLUTBridge::glutInit);
    MAP("glutInitDisplayMode", GLUTBridge::glutInitDisplayMode);
    MAP("glutInitWindowSize", GLUTBridge::glutInitWindowSize);
    MAP("glutInitWindowPosition", GLUTBridge::glutInitWindowPosition);
    MAP("glutEnterGameMode", GLUTBridge::glutEnterGameMode);
    MAP("glutLeaveGameMode", GLUTBridge::glutLeaveGameMode);
    MAP("glutCreateWindow", GLUTBridge::glutCreateWindow);
    MAP("glutMainLoop", GLUTBridge::glutMainLoop);
    MAP("glutDisplayFunc", GLUTBridge::glutDisplayFunc);
    MAP("glutIdleFunc", GLUTBridge::glutIdleFunc);
    MAP("glutPostRedisplay", GLUTBridge::glutPostRedisplay);
    MAP("glutSwapBuffers", GLUTBridge::glutSwapBuffers);
    MAP("glutGet", GLUTBridge::glutGet);
    MAP("glutSetCursor", GLUTBridge::glutSetCursor);
    MAP("glutGameModeString", GLUTBridge::glutGameModeString);
    MAP("glutBitmapCharacter", GLUTBridge::glutBitmapCharacter);
    MAP("glutBitmapWidth", GLUTBridge::glutBitmapWidth);
    MAP("glutMainLoopEvent", GLUTBridge::glutMainLoopEvent);
    MAP("glutJoystickFunc", GLUTBridge::glutJoystickFunc);
    MAP("glutPostRedisplay", GLUTBridge::glutPostRedisplay);
    MAP("glutSwapBuffers", GLUTBridge::glutSwapBuffers);
    MAP("glutGet", GLUTBridge::glutGet);
    MAP("glutSetCursor", GLUTBridge::glutSetCursor);
    MAP("glutGameModeString", GLUTBridge::glutGameModeString);
    MAP("glutBitmapCharacter", GLUTBridge::glutBitmapCharacter);
    MAP("glutBitmapWidth", GLUTBridge::glutBitmapWidth);
    MAP("glutKeyboardFunc", GLUTBridge::glutKeyboardFunc);
    MAP("glutKeyboardUpFunc", GLUTBridge::glutKeyboardUpFunc);
    MAP("glutSpecialUpFunc", GLUTBridge::glutSpecialUpFunc);
    MAP("glutMotionFunc", GLUTBridge::glutMotionFunc);
    MAP("glutMouseFunc", GLUTBridge::glutMouseFunc);
    MAP("glutPassiveMotionFunc", GLUTBridge::glutPassiveMotionFunc);
    MAP("glutEntryFunc", GLUTBridge::glutEntryFunc);
    MAP("glutMainLoopEvent", GLUTBridge::glutMainLoopEvent);
    MAP("glutJoystickFunc", GLUTBridge::glutJoystickFunc);
    MAP("glutSolidTeapot", GLUTBridge::glutSolidTeapot);
    MAP("glutWireTeapot", GLUTBridge::glutWireTeapot);
    MAP("glutSpecialFunc", GLUTBridge::glutSpecialFunc);
    MAP("glutReshapeFunc", GLUTBridge::glutReshapeFunc);
    MAP("glutSolidSphere", GLUTBridge::glutSolidSphere);
    MAP("glutWireSphere", GLUTBridge::glutWireSphere);
    MAP("glutWireCone", GLUTBridge::glutWireCone);
    MAP("glutSolidCone", GLUTBridge::glutSolidCone);
    MAP("glutWireCube", GLUTBridge::glutWireCube);
    MAP("glutSolidCube", GLUTBridge::glutSolidCube);
    MAP("glutVisibilityFunc", GLUTBridge::glutVisibilityFunc);
    MAP("glutExtensionSupported", GLUTBridge::glutExtensionSupported);
    MAP("glutGetModifiers", GLUTBridge::glutGetModifiers);

    MAP("gluPerspective", GLXBridge::gluPerspective);
    MAP("gluLookAt", GLXBridge::gluLookAt);
    MAP("gluPerspective", GLXBridge::gluPerspective);
    MAP("gluLookAt", GLXBridge::gluLookAt);
    MAP("gluOrtho2D", GLXBridge::gluOrtho2D);
    MAP("gluErrorString", GLXBridge::gluErrorString);

    // ============================================================
    // Missing Libc / Linux Specific / File I/O
    // ============================================================
    MAP("bzero", my_bzero);
    MAP("bcmp", my_bcmp);
    MAP("clock_gettime", my_clock_gettime);
    MAP("syscall", my_syscall);

    MAP("fsync", my_fsync);
    MAP("sync", my_sync);
    MAP("fdatasync", my_fdatasync);
    MAP("lseek", my_lseek);
    MAP("access", my_access);
    MAP("chmod", my_chmod);
    MAP("chdir", my_chdir);
    MAP("syslog", my_syslog);

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
    // MAP_OL("atan", (double (*)(double))atan);
    // MAP_OL("atan2", (double (*)(double, double))atan2);
    // MAP_OL("asin", (double (*)(double))asin);
    // MAP_OL("acos", (double (*)(double))acos);
    // MAP_OL("cos", (double (*)(double))cos);
    // MAP_OL("sin", (double (*)(double))sin);
    // MAP_OL("tan", (double (*)(double))tan);
    // MAP_OL("sqrt", (double (*)(double))sqrt);
    // MAP_OL("pow", (double (*)(double, double))pow);
    // MAP_OL("powf", (double (*)(double, double))pow);
    // MAP_OL("floor", (double (*)(double))floor);
    // MAP_OL("ceil", (double (*)(double))ceil);
    // MAP_OL("fmod", (double (*)(double, double))fmod);
    // MAP_OL("abs", (int (*)(int))abs);

    // MAP_OL("logf", (float (*)(float))log);
    // MAP_OL("log10", (double (*)(double))log10);
    // MAP_OL("sinhf", (float (*)(float))sinh);
    // MAP_OL("sinf", (float (*)(float))sin);
    // MAP_OL("hypot", (double (*)(double, double))hypot);
    // MAP_OL("tanhf", (float (*)(float))tanh);
    // MAP_OL("coshf", (float (*)(float))cosh);
    // MAP_OL("cosf", (float (*)(float))cos);
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
    MAP("strtod_l", LibcBridge::strtod_l_wrapper);
    MAP("__strtod_l", LibcBridge::strtod_l_wrapper);

    // C23 __isoc23_* aliases (glibc 2.38+)
    //    MAP_OL("__isoc23_strtoul", (unsigned long (*)(const char *, char **, int))strtoul);
    //    MAP_OL("__isoc23_strtol", (long (*)(const char *, char **, int))strtol);
    //    MAP_OL("__isoc23_strtoull", (unsigned long long (*)(const char *, char **, int))strtoull);
    //    MAP_OL("__isoc23_strtoll", (long long (*)(const char *, char **, int))strtoll);
    //    MAP_OL("__isoc23_strtod", (double (*)(const char *, char **))strtod);
    //    MAP_OL("__isoc23_strtof", (float (*)(const char *, char **))strtof);
    //    MAP_OL("__isoc23_strtold", (long double (*)(const char *, char **))strtold);
    MAP("__libc_start_main", my_libc_start_main);
    MAP("__cxa_atexit", my_cxa_atexit);
    MAP("__register_frame_info_bases", my___register_frame_info_bases);
    //    MAP("__cxa_vec_ctor", my_cxa_vec_ctor);
    //    MAP("__cxa_vec_dtor", my_cxa_vec_dtor);
    MAP("__assert_fail", __assert_fail);
    MAP("__errno_location", __errno_location);
    MAP("__errno_location", __errno_location);
    MAP("__libc_freeres", __libc_freeres);
    // MAP("__gxx_personality_v0", my_gxx_personality_v0);
    MAP("_ZdlPv", my_operator_delete);
    MAP("_Znwj", my_operator_new);
    MAP("_Znaj", my_operator_new_array);
    MAP("_ZdaPv", my_operator_delete); // operator delete[]
    // Missing C++ ABI (GCC 3.x / SGI STL)
    MAP("_ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_", my_Rb_tree_insert_and_rebalance);
    MAP("_ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_", my_Rb_tree_rebalance_for_erase);
    MAP("_ZNSt24__default_alloc_templateILb1ELi0EE8allocateEj", my_SGI_Alloc_Allocate);
    MAP("_ZNSt24__default_alloc_templateILb1ELi0EE10deallocateEPvj", my_SGI_Alloc_Deallocate);
    MAP("_ZNSt8ios_base4InitC1Ev", my_IosBaseInit_Ctor);
    MAP("_ZNSt8ios_base4InitD1Ev", my_IosBaseInit_Dtor);
    MAP("_ZNSt8ios_baseC2Ev", my_IosBase_Ctor);

    // GCC atomics
    MAP("_ZN9__gnu_cxx18__exchange_and_addEPVii", my_exchange_and_add);

    // std::string (GCC 3.x)
    MAP("_ZNSsC1Ev", my_StdString_DefaultCtor);
    MAP("_ZNSsC1EPKcRKSaIcE", my_StdString_ConstChar_Ctor);
    MAP("_ZNSsC1ERKSs", my_StdString_CopyCtor);
    MAP("_ZNSs4_Rep10_M_destroyERKSaIcE", my_StringRep_Destroy);
    MAP("_ZNSs6appendEPKcj", my_StdString_Append);
    MAP("_ZNSs7replaceEjjPKcj", my_StdString_Replace);
    MAP("_ZNSs9_M_mutateEjjj", my_StdString_Mutate);

    MAP("_ZNSs6assignEPKcj", my_StdString_AssignCharConst);
    // MAP("_ZNKSs4findEPKcjj", my_std_string_find);
    // MAP("_ZNSsC1ERKSsjj", my_std_string_substring_ctor);
    MAP("_ZNSs6assignERKSs", my_StdString_AssignCopy);
    // MAP("_ZNKSs5rfindEPKcjj", my_std_string_rfind);
    MAP("_ZNKSs7compareEPKc", my_StdString_CompareConstChar);
    // MAP("_ZNSs6appendEjc", my_std_string_append_char_count);
    // MAP("_ZNSs6appendERKSs", my_std_string_append_string);
    // MAP("_ZNSs6appendEPKcj", my_std_string_append);
    MAP("_ZNSsD1Ev", my_StdString_Dtor);
    MAP("_ZNSsD2Ev", my_StdString_Dtor);
    
    MAP("_ZStlsIcSt11char_traitsIcESaIcEERSt13basic_ostreamIT_T0_ES7_RSbIS4_S5_T1_E", my_StdString_Ostream_LShift);
    MAP("_ZStrsIcSt11char_traitsIcESaIcEERSt13basic_istreamIT_T0_ES7_RSbIS4_S5_T1_E", my_StdString_Istream_RShift);
    // MAP("_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEED1Ev", my_std_string_dtor);
    // MAP("_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEED2Ev", my_std_string_dtor);
    // MAP("_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1Ev", my_std_string_default_ctor);
    // MAP("_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1EPKcRKS3_", my_std_string_ctor);
    // MAP("_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1ERKS4_", my_std_string_copy_ctor);
    // MAP("_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC2IS3_EEPKcRKS3_", my_std_string_ctor);
    // MAP("_ZNSt11char_traitsIcE6lengthEPKc", strlen);
    // MAP("_ZNKSs5c_strEv", my_std_string_c_str);
    // MAP("_ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE5c_strEv", my_std_string_c_str);
    // MAP("_ZNKSs4sizeEv", my_std_string_size);
    // MAP("_ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE4sizeEv", my_std_string_size);
    // MAP("_ZNSsixEj", my_std_string_at);
    // MAP("_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEixEj", my_std_string_at);

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
    MAP("readlink", my_readlink);

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
    MAP("kill", LibcBridge::kill_wrapper);
    MAP("wait", LibcBridge::wait_wrapper);

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
    MAP("__xstat", fs_xstat);
    MAP("__xstat64", fs_xstat64);
    MAP("__lxstat", fs_lxstat);
    MAP("__fxstat", fs_fxstat);
    MAP("__fxstat64", fs_fxstat64);
    MAP("__xmknod", LibcBridge::__xmknod_wrapper);
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
    MAP("fgetpos", LibcBridge::fgetpos_wrapper);
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
    MAP("strtol", strtol);
    MAP("strtod", strtod);
    // MAP("isatty", LibcBridge::isatty_wrapper);

    MAP("strchr", LibcBridge::strchr_wrapper);
    MAP("strrchr", LibcBridge::strrchr_wrapper);
    MAP("strstr", LibcBridge::strstr_wrapper);
    MAP("strtok_r", LibcBridge::strtok_r_wrapper);

    // Standard Streams
    MAP_OL("stdout", stdout);
    MAP_OL("stderr", stderr);
    MAP_OL("stdin", stdin);
    MAP_OL("_IO_2_1_stdout_", stdout);
    MAP_OL("_IO_2_1_stderr_", stderr);
    MAP_OL("_IO_2_1_stdin_", stdin);

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
    MAP_OL("strpbrk", (char*(*)(char*, const char*))strpbrk);
    MAP("strcspn", strcspn);
    MAP("strspn", strspn);
    MAP("strtok", strtok);
    MAP("isdigit", isdigit);
    MAP("qsort", qsort);

    MAP("memchr", LibcBridge::memchr_wrapper);
    MAP("memmove", LibcBridge::memmove_wrapper);
    MAP("malloc", LibcBridge::malloc_wrapper);
    MAP("free", LibcBridge::free_wrapper);
    MAP("calloc", LibcBridge::calloc_wrapper);
    MAP("realloc", LibcBridge::realloc_wrapper);
    MAP("memalign", LibcBridge::memalign_wrapper);
    //    MAP("posix_memalign", my_posix_memalign);
    MAP("memcpy", memcpy);
    MAP("memset", memset);
    MAP("memcmp", memcmp);

    // Fortified libc functions (__chk variants)
    ////    MAP("__memcpy_chk", my_memcpy_chk);
    ////    MAP("__read_chk", my_read_chk);

    // Data symbols (MAP returns &variable — the address, not a function pointer)
    ////    MAP("__libc_single_threaded", __libc_single_threaded);

    // C++ libstdc++ internals
    ////    MAP("_ZN9__gnu_cxx21zoneinfo_dir_overrideEv", my_zoneinfo_dir_override);

    // Path / Environment
    MAP("getcwd", LibcBridge::getcwd_wrapper);
    MAP("realpath", LibcBridge::realpath_wrapper);
    MAP("remove", LibcBridge::remove_wrapper);
    MAP("mkdir", my_mkdir);
    MAP("rmdir", _rmdir);
    MAP("unlink", _unlink);
    MAP("access", _access);
    MAP("atoi", atoi);
    MAP("atof", atof);
    MAP("rand", LibcBridge::rand_wrapper);
    MAP("srand", LibcBridge::srand_wrapper);
    MAP("arc4random", my_arc4random);
    MAP("getenv", LibcBridge::getenv_wrapper);
    MAP("system", LibcBridge::system_wrapper);
    MAP("abort", LibcBridge::abort_wrapper);
    MAP("raise", LibcBridge::raise_wrapper);
    MAP("exit", LibcBridge::exit_wrapper);
    MAP("_exit", LibcBridge::_exit_wrapper);

    // Time
    MAP("time", LibcBridge::time_wrapper);  
    MAP("utime", LibcBridge::utime_wrapper);
    MAP("gettimeofday", LibcBridge::gettimeofday_wrapper);
    MAP("usleep", LibcBridge::usleep_wrapper);
    MAP("sleep", LibcBridge::sleep_wrapper);
    MAP("nanosleep", LibcBridge::nanosleep_wrapper);
    MAP("localtime", LibcBridge::localtime_wrapper);
    MAP("localtime_r", LibcBridge::localtime_r_wrapper);
    MAP("mktime", mktime);
    MAP("strftime", LibcBridge::strftime_wrapper);
    MAP("settimeofday", LibcBridge::settimeofday_wrapper);
    MAP("ctime_r", LibcBridge::ctime_r_wrapper);

    // Locale / WideChar
    MAP("setlocale", LibcBridge::setlocale_wrapper);
    MAP("__newlocale", my_newlocale);
    MAP("newlocale", my_newlocale);
    MAP("__freelocale", my_freelocale);
    MAP("freelocale", my_freelocale);
    MAP("__uselocale", my_uselocale);
    MAP("uselocale", my_uselocale);
    MAP("__duplocale", my_duplocale);
    MAP("duplocale", my_duplocale);
    // MAP("btowc", LibcBridge::btowc_wrapper);
    // MAP("wctob", LibcBridge::wctob_wrapper);
    // MAP("mbrtowc", LibcBridge::mbrtowc_wrapper);
    // MAP("wcrtomb", LibcBridge::wcrtomb_wrapper);
    // MAP("getwc", LibcBridge::getwc_wrapper);
    // MAP("putwc", LibcBridge::putwc_wrapper);
    // MAP("ungetwc", LibcBridge::ungetwc_wrapper);
    // MAP("wcslen", LibcBridge::wcslen_wrapper);
    // MAP("wmemcpy", LibcBridge::wmemcpy_wrapper);
    // MAP("wmemmove", LibcBridge::wmemmove_wrapper);
    // MAP("wmemset", LibcBridge::wmemset_wrapper);
    // MAP("wmemchr", LibcBridge::wmemchr_wrapper);
    // MAP("wmemcmp", LibcBridge::wmemcmp_wrapper);
    // MAP("wcscoll", LibcBridge::wcscoll_wrapper);
    // MAP("wcsxfrm", LibcBridge::wcsxfrm_wrapper);
    // MAP("wcsftime", LibcBridge::wcsftime_wrapper);
    // MAP("tolower", LibcBridge::tolower_wrapper);
    // MAP("toupper", LibcBridge::toupper_wrapper);
    // MAP("towlower", LibcBridge::towlower_wrapper);
    // MAP("towupper", LibcBridge::towupper_wrapper);
    // MAP("wctype", LibcBridge::wctype_wrapper);
    MAP("__wctype_l", LibcBridge::wctype_l_wrapper);
    MAP("iswctype", LibcBridge::iswctype_wrapper);
    // // MAP("wcpncpy", LibcBridge::wcpncpy_wrapper);
    // MAP("wcsftime", LibcBridge::wcsftime_wrapper);

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
    MAP("inet_pton", LibcBridge::inet_pton_wrapper);
    MAP("rand_r", LibcBridge::rand_r_wrapper);

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

    // Termios (Serial)
    MAP("tcgetattr", LibcBridge::tcgetattr_wrapper);
    MAP("tcsetattr", LibcBridge::tcsetattr_wrapper);
    MAP("tcflush", LibcBridge::tcflush_wrapper);
    MAP("tcdrain", LibcBridge::tcdrain_wrapper);
    MAP("cfgetispeed", my_stub_success);
    MAP("cfgetospeed", my_stub_success);
    MAP("cfsetispeed", LibcBridge::cfsetispeed_wrapper);
    MAP("cfsetospeed", LibcBridge::cfsetospeed_wrapper);
    MAP("cfsetspeed", LibcBridge::cfsetospeed_wrapper);
    // MAP("cfmakeraw", my_cfmakeraw);

    // Stubs
    MAP("poll", my_poll);
    MAP("sysctl", my_sysctl);
    MAP("iopl", my_stub_success);

    // XF86VidMode
    MAP("XF86VidModeQueryExtension", X11Bridge::XF86VidModeQueryExtension);
    MAP("XF86VidModeGetAllModeLines", X11Bridge::XF86VidModeGetAllModeLines);
    MAP("XF86VidModeGetModeLine", X11Bridge::XF86VidModeGetModeLine);
    MAP("XF86VidModeSwitchToMode", X11Bridge::XF86VidModeSwitchToMode);
    MAP("XF86VidModeSetViewPort", X11Bridge::XF86VidModeSetViewPort);
    MAP("XF86VidModeGetViewPort", X11Bridge::XF86VidModeGetViewPort);
    MAP("XF86VidModeLockModeSwitch", X11Bridge::XF86VidModeLockModeSwitch);
    MAP("XF86VidModeQueryVersion", X11Bridge::XF86VidModeQueryVersion);

    // X11 Misc
    MAP("XOpenDisplay", X11Bridge::XOpenDisplay);
    MAP("XCloseDisplay", X11Bridge::XCloseDisplay);
    MAP("XInitThreads", X11Bridge::XInitThreads);
    MAP("XLockDisplay", X11Bridge::XLockDisplay);
    MAP("XUnlockDisplay", X11Bridge::XUnlockDisplay);
    MAP("XSync", X11Bridge::XSync);
    MAP("XFlush", X11Bridge::XFlush);
    MAP("_XFlush", X11Bridge::XFlush);
    MAP("_XReply", X11Bridge::_XReply);
    MAP("XCreateWindow", X11Bridge::XCreateWindow);
    MAP("XDestroyWindow", X11Bridge::XDestroyWindow);
    MAP("XMapWindow", X11Bridge::XMapWindow);
    MAP("XGetWindowAttributes", X11Bridge::XGetWindowAttributes);
    MAP("XGetGeometry", X11Bridge::XGetGeometry);
    MAP("XMapRaised", X11Bridge::XMapRaised);
    MAP("XextFindDisplay", X11Bridge::XextFindDisplay);
    MAP("XMoveWindow", X11Bridge::XMoveWindow);
    MAP("XResizeWindow", X11Bridge::XResizeWindow);
    MAP("XPending", X11Bridge::XPending);
    MAP("XNextEvent", X11Bridge::XNextEvent);
    MAP("XTranslateCoordinates", X11Bridge::XTranslateCoordinates);
    MAP("XQueryPointer", X11Bridge::XQueryPointer);
    MAP("XWarpPointer", X11Bridge::XWarpPointer);
    MAP("XGrabPointer", X11Bridge::XGrabPointer);
    MAP("XUngrabPointer", X11Bridge::XUngrabPointer);
    MAP("XLookupString", X11Bridge::XLookupString);
    MAP("XStoreName", X11Bridge::XStoreName);
    MAP("XCreateColormap", X11Bridge::XCreateColormap);
    MAP("XSetTransientForHint", X11Bridge::XSetTransientForHint);

    MAP("XDisplayWidth", X11Bridge::XDisplayWidth);
    MAP("XDisplayHeight", X11Bridge::XDisplayHeight);
    MAP("XDisplayWidthMM", X11Bridge::XDisplayWidthMM);
    MAP("XDisplayHeightMM", X11Bridge::XDisplayHeightMM);
    MAP("XSetWMNormalHints", X11Bridge::XSetWMNormalHints);
    MAP_OL("XInternAtom", (Atom (*)(Display *, const char *, bool))X11Bridge::XInternAtom);
    MAP("XSetStandardProperties", X11Bridge::XSetStandardProperties);
    MAP("XRRGetScreenResources", X11Bridge::XRRGetScreenResources);
    MAP("XRRGetCrtcInfo", X11Bridge::XRRGetCrtcInfo);

    MAP("XParseColor", X11Bridge::XParseColor);
    MAP("XFree", X11Bridge::XFree);

    MAP("XSetErrorHandler", X11Bridge::XSetErrorHandler);
    MAP("XGetErrorText", X11Bridge::XGetErrorText);

    MAP("XCreatePixmapCursor", X11Bridge::XCreatePixmapCursor);
    MAP("XCreatePixmapFromBitmapData", X11Bridge::XCreatePixmapFromBitmapData);
    MAP("XFreePixmap", X11Bridge::XFreePixmap);
    MAP("XFreeCursor", X11Bridge::XFreeCursor);
    MAP("XDefineCursor", X11Bridge::XDefineCursor);

    MAP("XSetInputFocus", X11Bridge::XSetInputFocus);
    MAP("XSetWMProtocols", X11Bridge::XSetWMProtocols);
    MAP("XSetWMProperties", X11Bridge::XSetWMProperties);
    MAP("XStringListToTextProperty", X11Bridge::XStringListToTextProperty);
    MAP("XChangeProperty", X11Bridge::XChangeProperty);
    MAP("XSetCloseDownMode", X11Bridge::XSetCloseDownMode);
    MAP("XAutoRepeatOn", X11Bridge::XAutoRepeatOn);
    MAP("XAutoRepeatOff", X11Bridge::XAutoRepeatOff);
    MAP("XCreateBitmapFromData", X11Bridge::XCreateBitmapFromData);

    MAP("XGrabKeyboard", X11Bridge::XGrabKeyboard);
    MAP("XUngrabKeyboard", X11Bridge::XUngrabKeyboard);
    MAP("XKeysymToKeycode", X11Bridge::XKeysymToKeycode);
    MAP("XSendEvent", X11Bridge::XSendEvent);

    // ============================================================
    // GLX Functions
    // ============================================================
    MAP("glXChooseVisual", GLXBridge::ChooseVisual);
    MAP("glXCreateContext", GLXBridge::CreateContext);
    MAP("glXMakeCurrent", GLXBridge::MakeCurrent);
    MAP("glXCreateNewContext", GLXBridge::CreateNewContext);
    MAP("glXDestroyPbuffer", GLXBridge::DestroyPbuffer);
    MAP("glXCreatePbuffer", GLXBridge::CreatePbuffer);
    MAP("glXSwapBuffers", GLXBridge::SwapBuffers);
    MAP("glXDestroyContext", GLXBridge::DestroyContext);
    MAP("glXGetCurrentDisplay", GLXBridge::GetCurrentDisplay);
    MAP("glXGetCurrentDrawable", GLXBridge::GetCurrentDrawable);
    MAP("glXGetCurrentContext", GLXBridge::GetCurrentContext);
    MAP("glXChooseFBConfig", GLXBridge::ChooseFBConfig);
    MAP("glXGetVisualFromFBConfig", GLXBridge::GetVisualFromFBConfig);
    MAP("glXGetProcAddressARB", GLXBridge::GetProcAddress);
    MAP("glXGetProcAddress", GLXBridge::GetProcAddress);
    MAP("glXQueryExtension", GLXBridge::QueryExtension);
    MAP("glXSwapIntervalSGI", GLXBridge::SwapInterval);
    MAP("glXQueryExtension", GLXBridge::QueryExtension);
    MAP("glXQueryExtensionsString", GLXBridge::QueryExtensionsString);
    MAP("glXQueryServerString", GLXBridge::QueryServerString);
    MAP("glXQueryVersion", GLXBridge::QueryVersion);
    MAP("glXGetClientString", GLXBridge::GetClientString);
    MAP("glXIsDirect", GLXBridge::IsDirect);
    MAP("glXGetConfig", GLXBridge::GetConfig);

    // GLX SGI
    MAP("glXSwapIntervalSGI", GLXBridge::SwapIntervalSGI);
    MAP("glXGetVideoSyncSGI", GLXBridge::GetVideoSyncSGI);
    MAP("glXGetRefreshRateSGI", GLXBridge::GetRefreshRateSGI);

    // GLX SGIX / FBConfig Extensions
    MAP("glXChooseFBConfigSGIX", GLXBridge::ChooseFBConfigSGIX);
    MAP("glXGetFBConfigAttribSGIX", GLXBridge::GetFBConfigAttribSGIX);
    MAP("glXCreateContextWithConfigSGIX", GLXBridge::CreateContextWithConfigSGIX);
    MAP("glXCreateGLXPbufferSGIX", GLXBridge::CreateGLXPbufferSGIX);
    MAP("glXDestroyGLXPbufferSGIX", GLXBridge::DestroyGLXPbufferSGIX);

    // ============================================================
    // Pthreads
    // ============================================================

    // Thread functions
    MAP("pthread_create", PthreadEmu::pthread_create);
    MAP("pthread_join", PthreadEmu::pthread_join);
    MAP("pthread_detach", PthreadEmu::pthread_detach);
    MAP("pthread_exit", PthreadEmu::pthread_exit);
    MAP("pthread_self", PthreadEmu::pthread_self);
    MAP("pthread_equal", PthreadEmu::pthread_equal);
    MAP("pthread_cancel", PthreadEmu::pthread_cancel);

    // Thread attributes
    MAP("pthread_attr_init", PthreadEmu::pthread_attr_init);
    MAP("pthread_attr_destroy", PthreadEmu::pthread_attr_destroy);
    MAP("pthread_attr_setstacksize", PthreadEmu::pthread_attr_setstacksize);
    MAP("pthread_attr_getstacksize", PthreadEmu::pthread_attr_getstacksize);
    MAP("pthread_attr_setdetachstate", PthreadEmu::pthread_attr_setdetachstate);
    MAP("pthread_attr_getdetachstate", PthreadEmu::pthread_attr_getdetachstate);

    // Scheduling : Most of them are stubs
    MAP("pthread_attr_setschedparam", my_stub_success);
    MAP("pthread_attr_setschedpolicy", my_stub_success);
    MAP("pthread_setschedparam", PthreadEmu::pthread_setschedparam);
    MAP("pthread_getschedparam", PthreadEmu::pthread_getschedparam);
    MAP("sched_yield", PthreadEmu::sched_yield);
    MAP("sched_get_priority_min", my_sched_get_priority_min);
    MAP("sched_get_priority_max", my_sched_get_priority_max);
    MAP("sched_getaffinity", my_stub_success);
    MAP("sched_setaffinity", my_stub_success);

    // Mutex functions
    MAP("pthread_mutex_init", PthreadEmu::pthread_mutex_init);
    MAP("pthread_mutex_destroy", PthreadEmu::pthread_mutex_destroy);
    MAP("pthread_mutex_lock", PthreadEmu::pthread_mutex_lock);
    MAP("pthread_mutex_trylock", PthreadEmu::pthread_mutex_trylock);
    MAP("pthread_mutex_timedlock", PthreadEmu::pthread_mutex_timedlock);
    MAP("pthread_mutex_unlock", PthreadEmu::pthread_mutex_unlock);

    // Mutex attributes
    MAP("pthread_mutexattr_init", PthreadEmu::pthread_mutexattr_init);
    MAP("pthread_mutexattr_destroy", PthreadEmu::pthread_mutexattr_destroy);
    MAP("pthread_mutexattr_settype", PthreadEmu::pthread_mutexattr_settype);
    MAP("pthread_mutexattr_gettype", PthreadEmu::pthread_mutexattr_gettype);

    // Condition variable functions
    MAP("pthread_cond_init", PthreadEmu::pthread_cond_init);
    MAP("pthread_cond_destroy", PthreadEmu::pthread_cond_destroy);
    MAP("pthread_cond_wait", PthreadEmu::pthread_cond_wait);
    MAP("pthread_cond_timedwait", PthreadEmu::pthread_cond_timedwait);
    MAP("pthread_cond_signal", PthreadEmu::pthread_cond_signal);
    MAP("pthread_cond_broadcast", PthreadEmu::pthread_cond_broadcast);

    // Condition variable attributes
    MAP("pthread_condattr_init", PthreadEmu::pthread_condattr_init);
    MAP("pthread_condattr_destroy", PthreadEmu::pthread_condattr_destroy);

    // Read-write lock functions
    MAP("pthread_rwlock_init", PthreadEmu::pthread_rwlock_init);
    MAP("pthread_rwlock_destroy", PthreadEmu::pthread_rwlock_destroy);
    MAP("pthread_rwlock_rdlock", PthreadEmu::pthread_rwlock_rdlock);
    MAP("pthread_rwlock_tryrdlock", PthreadEmu::pthread_rwlock_tryrdlock);
    MAP("pthread_rwlock_wrlock", PthreadEmu::pthread_rwlock_wrlock);
    MAP("pthread_rwlock_trywrlock", PthreadEmu::pthread_rwlock_trywrlock);
    MAP("pthread_rwlock_unlock", PthreadEmu::pthread_rwlock_unlock);

    // Once control
    MAP("pthread_once", PthreadEmu::pthread_once);

    // TLS functions
    MAP("pthread_key_create", PthreadEmu::pthread_key_create);
    MAP("pthread_key_delete", PthreadEmu::pthread_key_delete);
    MAP("pthread_setspecific", PthreadEmu::pthread_setspecific);
    MAP("pthread_getspecific", PthreadEmu::pthread_getspecific);

    // Barrier functions
    MAP("pthread_barrier_init", PthreadEmu::pthread_barrier_init);
    MAP("pthread_barrier_destroy", PthreadEmu::pthread_barrier_destroy);
    MAP("pthread_barrier_wait", PthreadEmu::pthread_barrier_wait);

    MAP("signal", LibcBridge::signal_wrapper);

    // Spinlock functions
    MAP("pthread_spin_init", PthreadEmu::pthread_spin_init);
    MAP("pthread_spin_destroy", PthreadEmu::pthread_spin_destroy);
    MAP("pthread_spin_lock", PthreadEmu::pthread_spin_lock);
    MAP("pthread_spin_trylock", PthreadEmu::pthread_spin_trylock);
    MAP("pthread_spin_unlock", PthreadEmu::pthread_spin_unlock);

    // Semaphore functions (POSIX semaphores)
    MAP("sem_init", my_sem_init);
    MAP("sem_destroy", my_sem_destroy);
    MAP("sem_wait", my_sem_wait);
    MAP("sem_trywait", my_sem_trywait);
    MAP("sem_post", my_sem_post);
    MAP("sem_getvalue", my_sem_getvalue);
    // MAP("sem_open", my_sem_open);
    // MAP("sem_close", my_sem_close);

    // Intlo
    // MAP("bindtextdomain", my_bindtextdomain);
    // MAP("textdomain", my_textdomain);
    // MAP("gettext", my_gettext);

    // Sega API
    if (strncmp(name, "SEGAAPI_", 8) == 0)
    {
        if (strcmp(name, "SEGAAPI_Init") == 0)
            return (uintptr_t)&SEGAAPI_Init;
        if (strcmp(name, "SEGAAPI_Exit") == 0)
            return (uintptr_t)&SEGAAPI_Exit;
        if (strcmp(name, "SEGAAPI_Reset") == 0)
            return (uintptr_t)&SEGAAPI_Reset;
        if (strcmp(name, "SEGAAPI_Play") == 0)
            return (uintptr_t)&SEGAAPI_Play;
        if (strcmp(name, "SEGAAPI_Pause") == 0)
            return (uintptr_t)&SEGAAPI_Pause;
        if (strcmp(name, "SEGAAPI_Stop") == 0)
            return (uintptr_t)&SEGAAPI_Stop;
        if (strcmp(name, "SEGAAPI_PlayWithSetup") == 0)
            return (uintptr_t)&SEGAAPI_PlayWithSetup;
        if (strcmp(name, "SEGAAPI_GetPlaybackStatus") == 0)
            return (uintptr_t)&SEGAAPI_GetPlaybackStatus;
        if (strcmp(name, "SEGAAPI_SetPlaybackPosition") == 0)
            return (uintptr_t)&SEGAAPI_SetPlaybackPosition;
        if (strcmp(name, "SEGAAPI_GetPlaybackPosition") == 0)
            return (uintptr_t)&SEGAAPI_GetPlaybackPosition;
        if (strcmp(name, "SEGAAPI_SetReleaseState") == 0)
            return (uintptr_t)&SEGAAPI_SetReleaseState;
        if (strcmp(name, "SEGAAPI_CreateBuffer") == 0)
            return (uintptr_t)&SEGAAPI_CreateBuffer;
        if (strcmp(name, "SEGAAPI_DestroyBuffer") == 0)
            return (uintptr_t)&SEGAAPI_DestroyBuffer;
        if (strcmp(name, "SEGAAPI_UpdateBuffer") == 0)
            return (uintptr_t)&SEGAAPI_UpdateBuffer;
        if (strcmp(name, "SEGAAPI_SetFormat") == 0)
            return (uintptr_t)&SEGAAPI_SetFormat;
        if (strcmp(name, "SEGAAPI_GetFormat") == 0)
            return (uintptr_t)&SEGAAPI_GetFormat;
        if (strcmp(name, "SEGAAPI_SetSampleRate") == 0)
            return (uintptr_t)&SEGAAPI_SetSampleRate;
        if (strcmp(name, "SEGAAPI_GetSampleRate") == 0)
            return (uintptr_t)&SEGAAPI_GetSampleRate;
        if (strcmp(name, "SEGAAPI_SetPriority") == 0)
            return (uintptr_t)&SEGAAPI_SetPriority;
        if (strcmp(name, "SEGAAPI_GetPriority") == 0)
            return (uintptr_t)&SEGAAPI_GetPriority;
        if (strcmp(name, "SEGAAPI_SetUserData") == 0)
            return (uintptr_t)&SEGAAPI_SetUserData;
        if (strcmp(name, "SEGAAPI_GetUserData") == 0)
            return (uintptr_t)&SEGAAPI_GetUserData;
        if (strcmp(name, "SEGAAPI_SetSendRouting") == 0)
            return (uintptr_t)&SEGAAPI_SetSendRouting;
        if (strcmp(name, "SEGAAPI_GetSendRouting") == 0)
            return (uintptr_t)&SEGAAPI_GetSendRouting;
        if (strcmp(name, "SEGAAPI_SetSendLevel") == 0)
            return (uintptr_t)&SEGAAPI_SetSendLevel;
        if (strcmp(name, "SEGAAPI_GetSendLevel") == 0)
            return (uintptr_t)&SEGAAPI_GetSendLevel;
        if (strcmp(name, "SEGAAPI_SetChannelVolume") == 0)
            return (uintptr_t)&SEGAAPI_SetChannelVolume;
        if (strcmp(name, "SEGAAPI_GetChannelVolume") == 0)
            return (uintptr_t)&SEGAAPI_GetChannelVolume;
        if (strcmp(name, "SEGAAPI_SetNotificationFrequency") == 0)
            return (uintptr_t)&SEGAAPI_SetNotificationFrequency;
        if (strcmp(name, "SEGAAPI_SetNotificationPoint") == 0)
            return (uintptr_t)&SEGAAPI_SetNotificationPoint;
        if (strcmp(name, "SEGAAPI_ClearNotificationPoint") == 0)
            return (uintptr_t)&SEGAAPI_ClearNotificationPoint;
        if (strcmp(name, "SEGAAPI_SetStartLoopOffset") == 0)
            return (uintptr_t)&SEGAAPI_SetStartLoopOffset;
        if (strcmp(name, "SEGAAPI_GetStartLoopOffset") == 0)
            return (uintptr_t)&SEGAAPI_GetStartLoopOffset;
        if (strcmp(name, "SEGAAPI_SetEndLoopOffset") == 0)
            return (uintptr_t)&SEGAAPI_SetEndLoopOffset;
        if (strcmp(name, "SEGAAPI_GetEndLoopOffset") == 0)
            return (uintptr_t)&SEGAAPI_GetEndLoopOffset;
        if (strcmp(name, "SEGAAPI_SetEndOffset") == 0)
            return (uintptr_t)&SEGAAPI_SetEndOffset;
        if (strcmp(name, "SEGAAPI_GetEndOffset") == 0)
            return (uintptr_t)&SEGAAPI_GetEndOffset;
        if (strcmp(name, "SEGAAPI_SetLoopState") == 0)
            return (uintptr_t)&SEGAAPI_SetLoopState;
        if (strcmp(name, "SEGAAPI_GetLoopState") == 0)
            return (uintptr_t)&SEGAAPI_GetLoopState;
        if (strcmp(name, "SEGAAPI_SetSynthParam") == 0)
            return (uintptr_t)&SEGAAPI_SetSynthParam;
        if (strcmp(name, "SEGAAPI_GetSynthParam") == 0)
            return (uintptr_t)&SEGAAPI_GetSynthParam;
        if (strcmp(name, "SEGAAPI_SetSynthParamMultiple") == 0)
            return (uintptr_t)&SEGAAPI_SetSynthParamMultiple;
        if (strcmp(name, "SEGAAPI_GetSynthParamMultiple") == 0)
            return (uintptr_t)&SEGAAPI_GetSynthParamMultiple;
        if (strcmp(name, "SEGAAPI_SetGlobalEAXProperty") == 0)
            return (uintptr_t)&SEGAAPI_SetGlobalEAXProperty;
        if (strcmp(name, "SEGAAPI_GetGlobalEAXProperty") == 0)
            return (uintptr_t)&SEGAAPI_GetGlobalEAXProperty;
        if (strcmp(name, "SEGAAPI_SetSPDIFOutChannelStatus") == 0)
            return (uintptr_t)&SEGAAPI_SetSPDIFOutChannelStatus;
        if (strcmp(name, "SEGAAPI_GetSPDIFOutChannelStatus") == 0)
            return (uintptr_t)&SEGAAPI_GetSPDIFOutChannelStatus;
        if (strcmp(name, "SEGAAPI_SetSPDIFOutSampleRate") == 0)
            return (uintptr_t)&SEGAAPI_SetSPDIFOutSampleRate;
        if (strcmp(name, "SEGAAPI_GetSPDIFOutSampleRate") == 0)
            return (uintptr_t)&SEGAAPI_GetSPDIFOutSampleRate;
        if (strcmp(name, "SEGAAPI_SetSPDIFOutChannelRouting") == 0)
            return (uintptr_t)&SEGAAPI_SetSPDIFOutChannelRouting;
        if (strcmp(name, "SEGAAPI_GetSPDIFOutChannelRouting") == 0)
            return (uintptr_t)&SEGAAPI_GetSPDIFOutChannelRouting;
        if (strcmp(name, "SEGAAPI_SetIOVolume") == 0)
            return (uintptr_t)&SEGAAPI_SetIOVolume;
        if (strcmp(name, "SEGAAPI_GetIOVolume") == 0)
            return (uintptr_t)&SEGAAPI_GetIOVolume;
        if (strcmp(name, "SEGAAPI_SetLastStatus") == 0)
            return (uintptr_t)&SEGAAPI_SetLastStatus;
        if (strcmp(name, "SEGAAPI_GetLastStatus") == 0)
            return (uintptr_t)&SEGAAPI_GetLastStatus;
    }

    // Exception Handling (_Unwind_*)
    if (strncmp(name, "_Unwind_", 8) == 0)
    {
        void *proc = GetLibGccSymbol(name);
        if (proc)
            return (uintptr_t)proc;
    }

    // OpenGL (Direct)
    if (strncmp(name, "gl", 2) == 0 && strncmp(name, "glX", 3) != 0 && strncmp(name, "glut", 4) != 0)
    {
        // Route all GL calls through our GLHooks wrapper
        // This ensures CDECL -> STDCALL transition is handled correctly!
        void *proc = GLHooks::GetProcAddress(name);
        if (proc)
            return (uintptr_t)proc;
    }

    // MSYS2 Fallback
    void *msysProc = GetMsysSymbol(name);
    if (msysProc)
    {
        log_warn("DANGER: Symbol '%s' resolved to MSYS2 function! This will likely crash.", name);
        return (uintptr_t)msysProc;
    }

    // GCC Runtime (msys-gcc_s-1.dll) - general fallback
    void *gccProc = GetLibGccSymbol(name);
    if (gccProc)
    {
        log_debug("Resolved '%s' from GCC runtime (msys-gcc_s-1.dll)", name);
        return (uintptr_t)gccProc;
    }

    // --- Dynamic GLHooks Bridge ---
    if (strncmp(name, "gl", 2) == 0)
    {
        void *proc = GLHooks::GetProcAddress(name);
        if (proc)
            return (uintptr_t)proc;
    }

    // --- Runtime Logging for Missing Glut Functions ---
    if (strncmp(name, "glut", 4) == 0)
    {
        static std::set<std::string> missingGLSymbols;
        if (missingGLSymbols.find(name) == missingGLSymbols.end())
        {
            missingGLSymbols.insert(name);
            std::ofstream outfile;
            outfile.open("glutmissing.txt", std::ios_base::app); // Append mode
            if (outfile.is_open())
            {
                outfile << name << ",";
                outfile.close();
            }
        }
        log_warn("Symbol unresolved: %s -> Using stub", name);
        // return (uintptr_t)&UnimplementedStub;
        return CreateNamedStub(name);
    }

    // --- Runtime Logging for Missing OpenGL Functions ---
    if (strncmp(name, "gl", 2) == 0)
    {
        static std::set<std::string> missingGLSymbols;
        if (missingGLSymbols.find(name) == missingGLSymbols.end())
        {
            missingGLSymbols.insert(name);
            std::ofstream outfile;
            outfile.open("glmissing.txt", std::ios_base::app); // Append mode
            if (outfile.is_open())
            {
                outfile << name << ",";
                outfile.close();
            }
        }
        log_warn("Symbol unresolved: %s -> Using stub", name);
        // return (uintptr_t)&UnimplementedStub;
        return CreateNamedStub(name);
    }

    // --- Runtime Logging for Missing OpenGL Functions ---
    if (strncmp(name, "X", 1) == 0)
    {
        static std::set<std::string> missingGLSymbols;
        if (missingGLSymbols.find(name) == missingGLSymbols.end())
        {
            missingGLSymbols.insert(name);
            std::ofstream outfile;
            outfile.open("xmissing.txt", std::ios_base::app); // Append mode
            if (outfile.is_open())
            {
                outfile << name << ",";
                outfile.close();
            }
        }
        log_warn("Symbol unresolved: %s -> Using named stub", name);
        return CreateNamedStub(name);
        // return (uintptr_t)&UnimplementedStub;
    }

    static std::set<std::string> missingGLSymbols;
    if (missingGLSymbols.find(name) == missingGLSymbols.end())
    {
        missingGLSymbols.insert(name);
        std::ofstream outfile;
        outfile.open("missing.txt", std::ios_base::app); // Append mode
        if (outfile.is_open())
        {
            outfile << name << ",";
            outfile.close();
        }
    }
    log_warn("Symbol unresolved: %s -> Using stub", name);
    return CreateNamedStub(name); // Cambios
    // return (uintptr_t)&UnimplementedStub;
}

void SymbolResolver::ResolveAll(uintptr_t jmpRel, uintptr_t relTab, uintptr_t symTab, uintptr_t strTab, uint32_t pltRelSize,
                                uint32_t relSize, uint32_t relEnt, uintptr_t baseAddr)
{
    log_info(
        "SymbolResolver::ResolveAll params: jmpRel=0x%p, relTab=0x%p, symTab=0x%p, strTab=0x%p, pltRelSize=%d, relSize=%d, baseAddr=0x%p",
        (void *)jmpRel, (void *)relTab, (void *)symTab, (void *)strTab, pltRelSize, relSize, (void *)baseAddr);

    if (!symTab || !strTab)
    {
        log_error("Invalid Symbol/String Table parameters (symTab=%p, strTab=%p)", (void *)symTab, (void *)strTab);
        return;
    }

    size_t relCount = pltRelSize / sizeof(Elf32_Rel);
    log_info("Resolving %zu dynamic symbols...", relCount);

    int resolved = 0;
    int stubbed = 0;

    if (jmpRel && relCount > 0)
    {
        for (size_t i = 0; i < relCount; ++i)
        {
            Elf32_Rel *rel = (Elf32_Rel *)(jmpRel + i * sizeof(Elf32_Rel));

            bool trace = (i < 5);

            uint32_t symIdx = ELF32_R_SYM(rel->r_info);
            uintptr_t targetAddr = baseAddr + rel->r_offset;

            // if (trace)
            log_debug("Reloc %zu: Offset=0x%08X (Target=0x%p) Info=0x%08X SymIdx=%u", i, rel->r_offset, (void *)targetAddr, rel->r_info,
                      symIdx);

            Elf32_Sym *sym = (Elf32_Sym *)(symTab + symIdx * sizeof(Elf32_Sym));
            const char *name = (const char *)(strTab + sym->st_name);

            if (trace)
                log_debug("  -> Symbol Name: '%s'", name);

            uintptr_t addr = GetExternalAddr(name);
            if (IsStub(addr))
            {
                stubbed++;
            }
            else
            {
                resolved++;
            }

            // if (strncmp(name, "pthread", 7) != 0 && strncmp(name, "gl", 2) != 0)
            // {
            //     addr = CreateThunk(name, addr);
            // }

            if (rel->r_offset == 0)
            {
                log_error("CRITICAL: Relocation offset is NULL at index %zu. Skipping to avoid crash.", i);
                continue;
            }

            DWORD oldProtect;
            if (!VirtualProtect((LPVOID)targetAddr, 4, PAGE_READWRITE, &oldProtect))
            {
                log_error("VirtualProtect failed for address 0x%p (Error: %d). Segmentation fault likely imminent.", (void *)targetAddr,
                          GetLastError());
            }

            // Try writing wrapped in SEH if on MSVC, though user is on MinGW probably.
            // We'll just trust the log above to catch bad pointers before the crash.
            *(uintptr_t *)(targetAddr) = addr;

            VirtualProtect((LPVOID)targetAddr, 4, oldProtect, &oldProtect);
        }
    }

    // 2. Handle Data Relocations (Variables/Vtables) - DT_REL
    if (relTab && relSize > 0)
    {
        size_t entSize = relEnt ? relEnt : sizeof(Elf32_Rel);
        size_t relCount = relSize / entSize;
        log_info("Resolving %zu Data relocations...", relCount);

        for (size_t i = 0; i < relCount; ++i)
        {
            Elf32_Rel *rel = (Elf32_Rel *)(relTab + i * entSize);
            uint32_t type = ELF32_R_TYPE(rel->r_info);
            uint32_t symIdx = ELF32_R_SYM(rel->r_info);
            uintptr_t targetAddr = baseAddr + rel->r_offset;

            // Skip R_386_RELATIVE (Handled by ElfLoader)
            if (type == 8)
                continue;

            if (rel->r_offset == 0)
                continue;

            Elf32_Sym *sym = (Elf32_Sym *)(symTab + symIdx * sizeof(Elf32_Sym));
            const char *name = (const char *)(strTab + sym->st_name);

            // Log vtable/VTT symbols specifically
            bool isVtable = (name && (strncmp(name, "_ZTV", 4) == 0 || strncmp(name, "_ZTT", 4) == 0));
            if (isVtable)
            {
                log_info("  [VTABLE] Reloc %zu: '%s' type=%u, shndx=%u, st_value=0x%X, st_size=%u, target=0x%p", i, name, type,
                         sym->st_shndx, sym->st_value, sym->st_size, (void *)targetAddr);
                log_info("  [VTABLE]   Raw target content before: 0x%p (addend)", *(void **)targetAddr);
                log_info("  [VTABLE]   symTab=0x%p, strTab=0x%p, baseAddr=0x%p", (void *)symTab, (void *)strTab, (void *)baseAddr);

                // Dump first few dwords of what should be the vtable
                uint32_t *vtablePtr = (uint32_t *)targetAddr;
                log_info("  [VTABLE]   Vtable dump (first 4 dwords): 0x%08X 0x%08X 0x%08X 0x%08X", vtablePtr[0], vtablePtr[1], vtablePtr[2],
                         vtablePtr[3]);
            }

            // Check if symbol is defined internally (vtables, VTT, global variables, etc.)
            // SHN_UNDEF (0) means the symbol is undefined and needs external resolution
            uintptr_t symAddr;
            if (sym->st_shndx != SHN_UNDEF)
            {
                // Symbol is defined within this ELF - use its internal address
                // This handles vtables (_ZTV*), VTT (_ZTT*), and internal data
                // For ET_DYN, st_value is typically the offset from base
                symAddr = baseAddr + sym->st_value;
                if (isVtable)
                    log_info("  [VTABLE]   Internal: symAddr = baseAddr(0x%p) + st_value(0x%X) = 0x%p", (void *)baseAddr, sym->st_value,
                             (void *)symAddr);
            }
            else
            {
                // Symbol is undefined - resolve externally
                symAddr = GetExternalAddr(name);
                if (isVtable)
                    log_info("  [VTABLE]   External: GetExternalAddr returned 0x%p", (void *)symAddr);
            }

            DWORD oldProtect;
            if (VirtualProtect((LPVOID)targetAddr, 4, PAGE_READWRITE, &oldProtect))
            {
                // R_386_32: S + A (Symbol Address + Addend). Addend is stored at target.
                if (type == 1) // R_386_32
                {
                    uintptr_t oldVal = *(uintptr_t *)targetAddr;
                    *(uintptr_t *)targetAddr = symAddr + oldVal; // S + A
                    if (isVtable)
                        log_info("  [VTABLE]   R_386_32: *target = symAddr(0x%p) + addend(0x%X) = 0x%p", (void *)symAddr, oldVal,
                                 (void *)*(uintptr_t *)targetAddr);
                }
                // R_386_PC32: S + A - P (Symbol + Addend - PC)
                else if (type == 2) // R_386_PC32
                {
                    *(uintptr_t *)targetAddr += (symAddr - targetAddr);
                }
                // R_386_COPY: Copy data from symbol location to target location
                // Used for copying data (like vtables) from shared library to executable
                else if (type == 5) // R_386_COPY
                {
                    // For R_386_COPY, the symbol is defined in a shared library
                    // We need to resolve it externally and copy FROM there TO targetAddr
                    if (sym->st_size > 0)
                    {
                        // Resolve the symbol in shared libraries ONLY (skip main executable)
                        // R_386_COPY is specifically for copying from shared libs to executable
                        uintptr_t libAddr = SharedObjectManager::Instance().GetGlobalSymbolExcludingMain(name);

                        if (libAddr != 0)
                        {
                            // Dump source data before copying
                            if (isVtable)
                            {
                                log_info("  [VTABLE]   Source data in library (before copy):");
                                uint32_t *src = (uint32_t *)libAddr;
                                for (int i = 0; i < 4 && i * 4 < sym->st_size; ++i)
                                {
                                    log_info("     [0x%X] 0x%08X", libAddr + i * 4, src[i]);
                                }
                            }

                            // Copy from library to executable's target location
                            memcpy((void *)targetAddr, (void *)libAddr, sym->st_size);
                            if (isVtable)
                                log_info("  [VTABLE]   R_386_COPY: Copied %u bytes from library (0x%p) to 0x%p", sym->st_size,
                                         (void *)libAddr, (void *)targetAddr);

                            // Verify the copy worked
                            if (isVtable)
                                log_info("  [VTABLE]   Vtable dump after copy: 0x%08X 0x%08X 0x%08X 0x%08X", ((uint32_t *)targetAddr)[0],
                                         ((uint32_t *)targetAddr)[1], ((uint32_t *)targetAddr)[2], ((uint32_t *)targetAddr)[3]);
                        }
                        else
                        {
                            log_error("  [VTABLE]   R_386_COPY: Cannot find '%s' in any loaded library!", name);
                            log_error("  [VTABLE]   This symbol should be in libstdc++.so or libgcc_s.so");
                        }
                    }
                    else if (isVtable)
                    {
                        log_warn("  [VTABLE]   R_386_COPY: Symbol has st_size=0, nothing to copy");
                    }
                }
                // R_386_GLOB_DAT: S (Symbol Address). No addend - just write S.
                else if (type == 6) // R_386_GLOB_DAT
                {
                    *(uintptr_t *)targetAddr = symAddr;
                    if (isVtable)
                        log_info("  [VTABLE]   R_386_GLOB_DAT: *target = symAddr = 0x%p", (void *)symAddr);
                }
                else
                {
                    if (isVtable)
                        log_warn("  [VTABLE]   Unknown relocation type %u!", type);
                }

                if (isVtable)
                    log_info("  [VTABLE]   Raw target content after: 0x%p", *(void **)targetAddr);

                VirtualProtect((LPVOID)targetAddr, 4, oldProtect, &oldProtect);
            }
        }
    }

    log_info("Symbol resolution complete: %d resolved, %d stubbed", resolved, stubbed);
}
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <crtdbg.h>
#include "libcbridge.h"
#include "../src/core/log.h"
#include "../../hardware/lindberghdevice.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <csignal>
#include <mutex>
#include <stdarg.h>

// --- Virtual File Pointer Logic ---
#define VFILE_SENTINEL 0x80000000

static bool IsVirtual(FILE *f)
{
    return ((uintptr_t)f & VFILE_SENTINEL) != 0;
}

static int ToVfd(FILE *f)
{
    return (int)((uintptr_t)f & ~VFILE_SENTINEL);
}

static FILE *ToVPtr(int fd)
{
    return (FILE *)(uintptr_t)(fd | VFILE_SENTINEL);
}

// --- Helper Functions ---

static void ConvertPath(char *dst, const char *src, size_t size)
{
    if (!src || !dst)
        return;
    strncpy(dst, src, size);
    dst[size - 1] = '\0';
    for (size_t i = 0; dst[i]; i++)
    {
        if (dst[i] == '/')
            dst[i] = '\\';
    }
}

static std::mutex g_net_mutex;

static void InvalidParameterHandler(const wchar_t *expression, const wchar_t *function, const wchar_t *file, unsigned int line,
                                    uintptr_t pReserved)
{
    // log_warn("CRT Invalid Parameter detected");
}

// ========================================================================
// --- File Input/Output (stdio.h / io.h) ---
// ========================================================================

FILE *LibcBridge::fopen_wrapper(const char *filename, const char *mode)
{
    int linux_flags = 0;
    if (strchr(mode, 'r'))
        linux_flags = 0; // O_RDONLY
    if (strchr(mode, 'w'))
        linux_flags = 1; // O_WRONLY
    if (strchr(mode, '+'))
        linux_flags = 2; // O_RDWR

    // Try to open via LindberghDevice (virtual filesystem) first
    int fd = LindberghDevice::Instance().Open(filename, linux_flags);

    if (fd >= 0)
    {
        log_debug("LibcBridge: fopen mapped to Virtual FD %d for path: %s", fd, filename);
        return ToVPtr(fd);
    }

    // Fallback to Windows filesystem
    char winPath[MAX_PATH];
    ConvertPath(winPath, filename, MAX_PATH);
    FILE *f = fopen(winPath, mode);
    if (!f)
    {
        log_trace("fopen failed: %s (as %s) with mode %s", filename, winPath, mode);
    }
    return f;
}

FILE *LibcBridge::fopen64_wrapper(const char *filename, const char *mode)
{
    return fopen_wrapper(filename, mode);
}

int LibcBridge::fclose_wrapper(FILE *stream)
{
    if (!stream)
        return 0;

    if (IsVirtual(stream))
    {
        return LindberghDevice::Instance().Close(ToVfd(stream));
    }
    return fclose(stream);
}

int LibcBridge::creat_wrapper(const char *pathname, int mode)
{
    char winPath[MAX_PATH];
    ConvertPath(winPath, pathname, MAX_PATH);
    return _creat(winPath, mode);
}

size_t LibcBridge::fread_wrapper(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (IsVirtual(stream))
    {
        int bytesRead = LindberghDevice::Instance().Read(ToVfd(stream), ptr, (int)(size * nmemb));
        return (size > 0 && bytesRead > 0) ? (bytesRead / size) : 0;
    }
    return fread(ptr, size, nmemb, stream);
}

size_t LibcBridge::fwrite_wrapper(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (IsVirtual(stream))
    {
        int bytesWritten = LindberghDevice::Instance().Write(ToVfd(stream), ptr, (int)(size * nmemb));
        return (size > 0 && bytesWritten > 0) ? (bytesWritten / size) : 0;
    }

    if (!stream || !ptr)
        return 0;

    size_t ret = 0;
    _invalid_parameter_handler oldHandler = _set_invalid_parameter_handler(InvalidParameterHandler);

#ifdef _MSC_VER
    __try
    {
        ret = fwrite(ptr, size, nmemb, stream);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ret = 0;
    }
#else
    ret = fwrite(ptr, size, nmemb, stream);
#endif

    _set_invalid_parameter_handler(oldHandler);

    if (ret < nmemb && size * nmemb > 0)
    {
        size_t totalBytes = size * nmemb;
        if (totalBytes < 4096)
        {
            char *buf = (char *)malloc(totalBytes + 1);
            if (buf)
            {
                memcpy(buf, ptr, totalBytes);
                buf[totalBytes] = '\0';
                if (totalBytes > 0 && buf[totalBytes - 1] == '\n')
                    buf[totalBytes - 1] = '\0';
                log_game("[Output(fwrite)]: %s", buf);
                free(buf);
            }
        }
        // Stub implemention
        return nmemb;
    }

    return ret;
}

int LibcBridge::fseek_wrapper(FILE *stream, long offset, int whence)
{
    if (IsVirtual(stream))
    {
        return LindberghDevice::Instance().Seek(ToVfd(stream), offset, whence);
    }
    return fseek(stream, offset, whence);
}

long LibcBridge::ftell_wrapper(FILE *stream)
{
    if (IsVirtual(stream))
    {
        return LindberghDevice::Instance().Tell(ToVfd(stream));
    }
    return ftell(stream);
}

int LibcBridge::fseeko64_wrapper(FILE *stream, off64_t offset, int whence)
{
    if (IsVirtual(stream))
    {
        return LindberghDevice::Instance().Seek(ToVfd(stream), (long)offset, whence);
    }
    return _fseeki64(stream, offset, whence);
}

off64_t LibcBridge::ftello64_wrapper(FILE *stream)
{
    if (IsVirtual(stream))
    {
        return (off64_t)LindberghDevice::Instance().Tell(ToVfd(stream));
    }
    return _ftelli64(stream);
}

void LibcBridge::rewind_wrapper(FILE *stream)
{
    fseek_wrapper(stream, 0, SEEK_SET);
}

int LibcBridge::fgetc_wrapper(FILE *stream)
{
    if (IsVirtual(stream))
    {
        uint8_t c;
        if (LindberghDevice::Instance().Read(ToVfd(stream), &c, 1) == 1)
            return (int)c;
        return EOF;
    }
    return fgetc(stream);
}

char *LibcBridge::fgets_wrapper(char *s, int size, FILE *stream)
{
    if (IsVirtual(stream))
    {
        int i = 0;
        for (; i < size - 1; i++)
        {
            int c = fgetc_wrapper(stream);
            if (c == EOF)
                break;
            s[i] = (char)c;
            if (c == '\n')
            {
                i++;
                break;
            }
        }
        s[i] = '\0';
        return (i == 0) ? nullptr : s;
    }
    return fgets(s, size, stream);
}

int LibcBridge::fputs_wrapper(const char *s, FILE *stream)
{
    if (IsVirtual(stream))
    {
        size_t len = strlen(s);
        return (LindberghDevice::Instance().Write(ToVfd(stream), s, (int)len) == (int)len) ? 0 : EOF;
    }

    if (!stream || !s)
        return EOF;

    int ret = EOF;
    _invalid_parameter_handler oldHandler = _set_invalid_parameter_handler(InvalidParameterHandler);
#ifdef _MSC_VER
    __try
    {
        ret = fputs(s, stream);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ret = EOF;
    }
#else
    ret = fputs(s, stream);
#endif
    _set_invalid_parameter_handler(oldHandler);

    if (ret == EOF)
    {
        log_game("[Output(fputs)]: %s", s);
        return 0;
    }
    return ret;
}

int LibcBridge::fputc_wrapper(int c, FILE *stream)
{
    if (IsVirtual(stream))
    {
        uint8_t ch = (uint8_t)c;
        return (LindberghDevice::Instance().Write(ToVfd(stream), &ch, 1) == 1) ? c : EOF;
    }

    if (!stream)
        return EOF;

    int ret = EOF;
    _invalid_parameter_handler oldHandler = _set_invalid_parameter_handler(InvalidParameterHandler);
#ifdef _MSC_VER
    __try
    {
        ret = fputc(c, stream);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ret = EOF;
    }
#else
    ret = fputc(c, stream);
#endif
    _set_invalid_parameter_handler(oldHandler);

    if (ret == EOF)
    {
        return c;
    }
    return ret;
}

int LibcBridge::putc_wrapper(int c, FILE *stream)
{
    return fputc_wrapper(c, stream);
}

int LibcBridge::putchar_wrapper(int c)
{
    return putchar(c);
}

int LibcBridge::puts_wrapper(const char *s)
{
    return puts(s);
}

int LibcBridge::ungetc_wrapper(int c, FILE *stream)
{
    if (IsVirtual(stream))
        return EOF; // Not supported for virtual files
    return ungetc(c, stream);
}

int LibcBridge::getc_wrapper(FILE *stream)
{
    return fgetc_wrapper(stream);
}

int LibcBridge::feof_wrapper(FILE *stream)
{
    if (IsVirtual(stream))
        return 0; // TODO: Implement proper EOF check for virtual files
    return feof(stream);
}

int LibcBridge::ferror_wrapper(FILE *stream)
{
    if (IsVirtual(stream))
        return 0;
    return ferror(stream);
}

int LibcBridge::fflush_wrapper(FILE *stream)
{
    if (IsVirtual(stream))
        return 0;
    return fflush(stream);
}

int LibcBridge::fileno_wrapper(FILE *stream)
{
    if (IsVirtual(stream))
        return ToVfd(stream);
    return _fileno(stream);
}

FILE *LibcBridge::fdopen_wrapper(int fd, const char *mode)
{
    // If fd is >= 100, we assume it's a virtual FD managed by LindberghDevice
    if (fd >= 100)
        return ToVPtr(fd);
    return _fdopen(fd, mode);
}

void LibcBridge::perror_wrapper(const char *s)
{
    perror(s);
}

int LibcBridge::setvbuf_wrapper(FILE *stream, char *buf, int mode, size_t size)
{
    if (IsVirtual(stream))
        return 0;
    return setvbuf(stream, buf, mode, size);
}

int LibcBridge::remove_wrapper(const char *pathname)
{
    char winPath[MAX_PATH];
    ConvertPath(winPath, pathname, MAX_PATH);
    return remove(winPath);
}

// ========================================================================
// --- Formatted I/O ---
// ========================================================================

int LibcBridge::printf_wrapper(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (size > 0)
    {
        char *buffer = (char *)malloc(size + 1);
        if (buffer)
        {
            vsnprintf(buffer, size + 1, format, args);
            // Remove trailing newline for cleaner logging
            if (size > 0 && buffer[size - 1] == '\n')
                buffer[size - 1] = '\0';
            log_game("%s", buffer);
            free(buffer);
        }
    }
    va_end(args);
    return size;
}

int LibcBridge::vprintf_wrapper(const char *format, va_list ap)
{
    va_list args_copy;
    va_copy(args_copy, ap);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (size > 0)
    {
        char *buffer = (char *)malloc(size + 1);
        if (buffer)
        {
            vsnprintf(buffer, size + 1, format, ap);
            if (size > 0 && buffer[size - 1] == '\n')
                buffer[size - 1] = '\0';
            log_game("%s", buffer);
            free(buffer);
        }
    }
    return size;
}

int LibcBridge::fprintf_wrapper(FILE *stream, const char *format, ...)
{
    if (IsVirtual(stream))
        return 0;

    va_list args;
    va_start(args, format);

    int size = _vscprintf(format, args);
    int ret = 0;

    if (size > 0)
    {
        char *buffer = (char *)malloc(size + 1);
        if (buffer)
        {
            vsnprintf(buffer, size + 1, format, args);

            bool success = false;
            if (stream != nullptr)
            {
                _invalid_parameter_handler oldHandler = _set_invalid_parameter_handler(InvalidParameterHandler);
#ifdef _MSC_VER
                __try
                {
                    ret = vfprintf(stream, format, args);
                    success = true;
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    success = false;
                }
#else
                ret = vfprintf(stream, format, args);
                success = true;
#endif
                _set_invalid_parameter_handler(oldHandler);
            }

            if (!success)
            {
                if (buffer[size - 1] == '\n')
                    buffer[size - 1] = '\0';
                log_game("[Output]: %s", buffer);
                ret = size;
            }
            free(buffer);
        }
    }

    va_end(args);
    return ret;
}

int LibcBridge::vfprintf_wrapper(FILE *stream, const char *format, va_list ap)
{
    if (IsVirtual(stream))
        return 0;

    va_list args_copy;
    va_copy(args_copy, ap);
    int size = _vscprintf(format, args_copy);
    va_end(args_copy);

    int ret = 0;
    if (size > 0)
    {
        char *buffer = (char *)malloc(size + 1);
        if (buffer)
        {
            vsnprintf(buffer, size + 1, format, ap);

            bool success = false;
            if (stream != nullptr)
            {
                _invalid_parameter_handler oldHandler = _set_invalid_parameter_handler(InvalidParameterHandler);
#ifdef _MSC_VER
                __try
                {
                    ret = vfprintf(stream, format, ap);
                    success = true;
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    success = false;
                }
#else
                ret = vfprintf(stream, format, ap);
                success = true;
#endif
                _set_invalid_parameter_handler(oldHandler);
            }

            if (!success)
            {
                if (buffer[size - 1] == '\n')
                    buffer[size - 1] = '\0';
                log_game("[Output]: %s", buffer);
                ret = size;
            }
            free(buffer);
        }
    }
    return ret;
}

int LibcBridge::fscanf_wrapper(FILE *stream, const char *format, ...)
{
    if (IsVirtual(stream))
        return 0;
    if (!stream)
        return 0;

    va_list args;
    va_start(args, format);
    int ret = 0;
    _invalid_parameter_handler oldHandler = _set_invalid_parameter_handler(InvalidParameterHandler);
#ifdef _MSC_VER
    __try
    {
        ret = vfscanf(stream, format, args);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ret = 0;
    }
#else
    ret = vfscanf(stream, format, args);
#endif
    _set_invalid_parameter_handler(oldHandler);
    va_end(args);
    return ret;
}

int LibcBridge::sscanf_wrapper(const char *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsscanf(str, format, args);
    va_end(args);
    return ret;
}

int LibcBridge::snprintf_wrapper(char *str, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = _vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}

int LibcBridge::vsnprintf_wrapper(char *str, size_t size, const char *format, va_list ap)
{
    return _vsnprintf(str, size, format, ap);
}

int LibcBridge::vsprintf_wrapper(char *str, const char *format, va_list ap)
{
    return vsprintf(str, format, ap);
}

int LibcBridge::sprintf_wrapper(char *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsprintf(str, format, args);
    va_end(args);
    return ret;
}

// ========================================================================
// --- String & Memory Operations ---
// ========================================================================

char *LibcBridge::strchr_wrapper(const char *s, int c)
{
    return (char *)strchr(s, c);
}
char *LibcBridge::strrchr_wrapper(const char *s, int c)
{
    return (char *)strrchr(s, c);
}
char *LibcBridge::strstr_wrapper(const char *haystack, const char *needle)
{
    return (char *)strstr(haystack, needle);
}
void *LibcBridge::memchr_wrapper(const void *s, int c, size_t n)
{
    return (void *)memchr(s, c, n);
}
char *LibcBridge::strtok_r_wrapper(char *s, const char *delim, char **saveptr)
{
    return strtok_s(s, delim, saveptr);
}
char *LibcBridge::strncat_wrapper(char *dest, const char *src, size_t n)
{
    return strncat(dest, src, n);
}
char *LibcBridge::strncpy_wrapper(char *dest, const char *src, size_t n)
{
    return strncpy(dest, src, n);
}
char *LibcBridge::strerror_wrapper(int errnum)
{
    return strerror(errnum);
}

char *LibcBridge::strdup_wrapper(const char *s)
{
    if (!s)
        return nullptr;
    size_t len = strlen(s) + 1;
    void *ptr = _aligned_malloc(len, 16);
    if (ptr)
        memcpy(ptr, s, len);
    return (char *)ptr;
}

int LibcBridge::strcasecmp_wrapper(const char *s1, const char *s2)
{
    return _stricmp(s1, s2);
}
void *LibcBridge::memmove_wrapper(void *dest, const void *src, size_t n)
{
    return memmove(dest, src, n);
}
int LibcBridge::strcoll_wrapper(const char *s1, const char *s2)
{
    return strcoll(s1, s2);
}
size_t LibcBridge::strxfrm_wrapper(char *dest, const char *src, size_t n)
{
    return strxfrm(dest, src, n);
}

void *LibcBridge::malloc_wrapper(size_t size)
{
    return _aligned_malloc(size, 16);
}
void LibcBridge::free_wrapper(void *ptr)
{
    _aligned_free(ptr);
}

void *LibcBridge::calloc_wrapper(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = _aligned_malloc(total, 16);
    if (ptr)
        memset(ptr, 0, total);
    return ptr;
}

void *LibcBridge::realloc_wrapper(void *ptr, size_t size)
{
    return _aligned_realloc(ptr, size, 16);
}
void *LibcBridge::memalign_wrapper(size_t alignment, size_t size)
{
    return _aligned_malloc(size, alignment);
}

// ========================================================================
// --- Path & Directory Management ---
// ========================================================================

char *LibcBridge::getcwd_wrapper(char *buf, size_t size)
{
    if (buf != nullptr)
        return _getcwd(buf, (int)size);
    char *tmp = _getcwd(NULL, 0);
    if (!tmp)
        return nullptr;
    size_t len = strlen(tmp) + 1;
    if (size > 0 && size < len)
        len = size;
    char *aligned_ptr = (char *)_aligned_malloc(len, 16);
    if (aligned_ptr)
    {
        strncpy(aligned_ptr, tmp, len);
        aligned_ptr[len - 1] = '\0';
    }
    free(tmp);
    return aligned_ptr;
}

char *LibcBridge::realpath_wrapper(const char *path, char *resolved_path)
{
    char winPath[MAX_PATH];
    ConvertPath(winPath, path, MAX_PATH);
    if (resolved_path)
        return _fullpath(resolved_path, winPath, MAX_PATH);
    char buffer[MAX_PATH];
    if (_fullpath(buffer, winPath, MAX_PATH))
    {
        size_t len = strlen(buffer) + 1;
        void *ptr = _aligned_malloc(len, 16);
        if (ptr)
        {
            memcpy(ptr, buffer, len);
            return (char *)ptr;
        }
    }
    return nullptr;
}

// ========================================================================
// --- Time & Clock Functions ---
// ========================================================================

int LibcBridge::utime_wrapper(const char *filename, const void *times)
{
    char winPath[MAX_PATH];
    ConvertPath(winPath, filename, MAX_PATH);
    return _utime(winPath, (struct _utimbuf *)times);
}

size_t LibcBridge::strftime_wrapper(char *s, size_t max, const char *format, const struct tm *tm)
{
    return strftime(s, max, format, tm);
}
struct tm *LibcBridge::localtime_wrapper(const time_t *timep)
{
    return localtime(timep);
}
time_t LibcBridge::mktime_wrapper(struct tm *tm)
{
    return mktime(tm);
}

int LibcBridge::nanosleep_wrapper(const struct timespec *req, struct timespec *rem)
{
    if (req)
    {
        DWORD ms = (DWORD)(req->tv_sec * 1000 + (req->tv_nsec / 1000000));
        Sleep(ms);
    }
    return 0;
}

int LibcBridge::settimeofday_wrapper(const struct timeval *tv, const void *tz)
{
    if (!tv)
        return -1;
    FILETIME ft;
    unsigned __int64 ticks = (unsigned __int64)tv->tv_sec * 10000000ULL + (unsigned __int64)tv->tv_usec * 10ULL + 116444736000000000ULL;
    ft.dwLowDateTime = (DWORD)ticks;
    ft.dwHighDateTime = (DWORD)(ticks >> 32);
    SYSTEMTIME st;
    if (FileTimeToSystemTime(&ft, &st))
        return SetSystemTime(&st) ? 0 : -1;
    return -1;
}

int LibcBridge::gettimeofday_wrapper(struct timeval *tv, void *tz)
{
    if (tv)
    {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        unsigned __int64 t = (unsigned __int64)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
        t -= 116444736000000000ULL;
        t /= 10;
        tv->tv_sec = (long)(t / 1000000);
        tv->tv_usec = (long)(t % 1000000);
    }
    return 0;
}

int LibcBridge::usleep_wrapper(unsigned int usec)
{
    Sleep(usec / 1000);
    return 0;
}
unsigned int LibcBridge::sleep_wrapper(unsigned int seconds)
{
    Sleep(seconds * 1000);
    return 0;
}
struct tm *LibcBridge::localtime_r_wrapper(const time_t *timep, struct tm *result)
{
    if (localtime_s(result, timep) == 0)
        return result;
    return nullptr;
}

char *LibcBridge::ctime_r_wrapper(const time_t *timep, char *buf)
{
    if (ctime_s(buf, 26, timep) == 0)
    {
        return buf;
    }
    return nullptr;
}

// ========================================================================
// --- Standard Library & System ---
// ========================================================================

int LibcBridge::rand_r_wrapper(unsigned int *seedp)
{
    unsigned int next = *seedp;
    next = next * 1103515245 + 12345;
    *seedp = next;
    return (unsigned int)(next / 65536) % 32768;
}

int LibcBridge::rand_wrapper()
{
    return rand();
}
void LibcBridge::srand_wrapper(unsigned int seed)
{
    srand(seed);
}
char *LibcBridge::getenv_wrapper(const char *name)
{
    return getenv(name);
}
int LibcBridge::system_wrapper(const char *command)
{
    log_debug("system(\"%s\")", command);
    return system(command);
}
void LibcBridge::abort_wrapper()
{
    log_fatal("abort() called!");
    abort();
}
int LibcBridge::raise_wrapper(int sig)
{
    log_warn("raise(%d) called", sig);
    return raise(sig);
}
void LibcBridge::_exit_wrapper(int status)
{
    log_info("_exit(%d)", status);
    _exit(status);
}
void LibcBridge::exit_wrapper(int status)
{
    log_info("exit(%d)", status);
    exit(status);
}

struct linux_stat
{
    uint32_t st_dev;
    uint16_t __pad1;
    uint32_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_rdev;
    uint16_t __pad2;
    uint32_t st_size;
    uint32_t st_blksize;
    uint32_t st_blocks;
    uint32_t st_atime;
    uint32_t st_atime_nsec;
    uint32_t st_mtime;
    uint32_t st_mtime_nsec;
    uint32_t st_ctime;
    uint32_t st_ctime_nsec;
    uint32_t __unused4;
    uint32_t __unused5;
};

int LibcBridge::fxstat_wrapper(int ver, int fd, void *stat_buf)
{
    struct _stat64 st;
    if (_fstat64(fd, &st) != 0)
        return -1;
    struct linux_stat *ls = (struct linux_stat *)stat_buf;
    memset(ls, 0, sizeof(linux_stat));
    ls->st_mode = (uint32_t)st.st_mode;
    ls->st_size = (uint32_t)st.st_size;
    ls->st_atime = (uint32_t)st.st_atime;
    ls->st_mtime = (uint32_t)st.st_mtime;
    ls->st_ctime = (uint32_t)st.st_ctime;
    ls->st_nlink = (uint32_t)st.st_nlink;
    return 0;
}

// ========================================================================
// --- Network & Socket Utilities ---
// ========================================================================

int LibcBridge::inet_aton_wrapper(const char *cp, struct in_addr *inp)
{
    unsigned long addr = inet_addr(cp);
    if (addr == INADDR_NONE && strcmp(cp, "255.255.255.255") != 0)
        return 0;
    if (inp)
        inp->s_addr = addr;
    return 1;
}

int LibcBridge::inet_pton_wrapper(int af, const char *src, void *dst)
{
    return InetPtonA(af, src, dst);
}

int LibcBridge::gethostbyname_r_wrapper(const char *name, void *ret, char *buf, size_t buflen, void **result, int *h_errnop)
{
    std::lock_guard<std::mutex> lock(g_net_mutex);
    struct hostent *he = gethostbyname(name);
    if (!he)
    {
        if (h_errnop)
            *h_errnop = h_errno;
        *result = nullptr;
        return -1;
    }
    memcpy(ret, he, sizeof(struct hostent));
    *result = ret;
    return 0;
}

int LibcBridge::gethostbyaddr_r_wrapper(const void *addr, int len, int type, void *ret, char *buf, size_t buflen, void **result,
                                        int *h_errnop)
{
    std::lock_guard<std::mutex> lock(g_net_mutex);
    struct hostent *he = gethostbyaddr((const char *)addr, len, type);
    if (!he)
    {
        if (h_errnop)
            *h_errnop = h_errno;
        *result = nullptr;
        return -1;
    }
    memcpy(ret, he, sizeof(struct hostent));
    *result = ret;
    return 0;
}

// ========================================================================
// --- Locale & Wide Character Support ---
// ========================================================================

char *LibcBridge::setlocale_wrapper(int category, const char *locale)
{
    return setlocale(category, locale);
}
wint_t LibcBridge::btowc_wrapper(int c)
{
    return btowc(c);
}
int LibcBridge::wctob_wrapper(wint_t c)
{
    return wctob(c);
}
size_t LibcBridge::mbrtowc_wrapper(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
    return mbrtowc(pwc, s, n, ps);
}
size_t LibcBridge::wcrtomb_wrapper(char *s, wchar_t wc, mbstate_t *ps)
{
    return wcrtomb(s, wc, ps);
}
wint_t LibcBridge::getwc_wrapper(FILE *stream)
{
    return getwc(stream);
}
wint_t LibcBridge::putwc_wrapper(wchar_t wc, FILE *stream)
{
    return putwc(wc, stream);
}
wint_t LibcBridge::ungetwc_wrapper(wint_t wc, FILE *stream)
{
    return ungetwc(wc, stream);
}
size_t LibcBridge::wcslen_wrapper(const wchar_t *s)
{
    return wcslen(s);
}
wchar_t *LibcBridge::wmemcpy_wrapper(wchar_t *dest, const wchar_t *src, size_t n)
{
    return wmemcpy(dest, src, n);
}
wchar_t *LibcBridge::wmemmove_wrapper(wchar_t *dest, const wchar_t *src, size_t n)
{
    return wmemmove(dest, src, n);
}
wchar_t *LibcBridge::wmemset_wrapper(wchar_t *wcs, wchar_t wc, size_t n)
{
    return wmemset(wcs, wc, n);
}
wchar_t *LibcBridge::wmemchr_wrapper(const wchar_t *wcs, wchar_t wc, size_t n)
{
    return (wchar_t *)wmemchr(wcs, wc, n);
}
int LibcBridge::wmemcmp_wrapper(const wchar_t *s1, const wchar_t *s2, size_t n)
{
    return wmemcmp(s1, s2, n);
}
int LibcBridge::wcscoll_wrapper(const wchar_t *s1, const wchar_t *s2)
{
    return wcscoll(s1, s2);
}
size_t LibcBridge::wcsxfrm_wrapper(wchar_t *dest, const wchar_t *src, size_t n)
{
    return wcsxfrm(dest, src, n);
}
size_t LibcBridge::wcsftime_wrapper(wchar_t *wcs, size_t maxsize, const wchar_t *format, const struct tm *tm)
{
    return wcsftime(wcs, maxsize, format, tm);
}
int LibcBridge::tolower_wrapper(int c)
{
    return tolower(c);
}
int LibcBridge::toupper_wrapper(int c)
{
    return toupper(c);
}
wint_t LibcBridge::towlower_wrapper(wint_t wc)
{
    return towlower(wc);
}
wint_t LibcBridge::towupper_wrapper(wint_t wc)
{
    return towupper(wc);
}

wctype_t LibcBridge::wctype_wrapper(const char *property)
{
    if (strcmp(property, "alnum") == 0)
        return _ALPHA | _DIGIT;
    if (strcmp(property, "alpha") == 0)
        return _ALPHA;
    if (strcmp(property, "cntrl") == 0)
        return _CONTROL;
    if (strcmp(property, "digit") == 0)
        return _DIGIT;
    if (strcmp(property, "graph") == 0)
        return _PUNCT | _ALPHA | _DIGIT;
    if (strcmp(property, "lower") == 0)
        return _LOWER;
    if (strcmp(property, "print") == 0)
        return _BLANK | _PUNCT | _ALPHA | _DIGIT;
    if (strcmp(property, "punct") == 0)
        return _PUNCT;
    if (strcmp(property, "space") == 0)
        return _SPACE;
    if (strcmp(property, "upper") == 0)
        return _UPPER;
    if (strcmp(property, "xdigit") == 0)
        return _HEX;
    return 0;
}

int LibcBridge::iswctype_wrapper(wint_t wc, wctype_t desc)
{
    return iswctype(wc, desc);
}

// ========================================================================
// --- Termios (Serial Port) Support Implementation ---
// ========================================================================

static std::map<int, LibcBridge::linux_termios> g_termios_state;

int LibcBridge::tcgetattr_wrapper(int fd, struct linux_termios *termios_p)
{
    if (!termios_p)
        return -1;

    // If state exists, return it; otherwise return default "raw-ish" mode
    if (g_termios_state.find(fd) != g_termios_state.end())
    {
        *termios_p = g_termios_state[fd];
    }
    else
    {
        memset(termios_p, 0, sizeof(linux_termios));
        // Default flags typical for JVS communication
        termios_p->c_cflag = 0x1000 | 0x0800 | 0x0030; // CLOCAL | CREAD | CS8
        termios_p->c_ispeed = 115200;
        termios_p->c_ospeed = 115200;
    }
    return 0;
}

int LibcBridge::tcsetattr_wrapper(int fd, int optional_actions, const struct linux_termios *termios_p)
{
    if (!termios_p)
        return -1;
    // Store state to appease the game
    g_termios_state[fd] = *termios_p;
    log_debug("tcsetattr: fd=%d, speed=%d, flags=0x%x", fd, termios_p->c_ospeed, termios_p->c_cflag);
    return 0;
}

int LibcBridge::tcflush_wrapper(int fd, int queue_selector)
{
    // Ideally map to PurgeComm, but returning 0 (success) is sufficient for now
    return 0;
}

int LibcBridge::tcdrain_wrapper(int fd)
{
    // Wait for transmission. Sleep briefly to simulate IO delay.
    Sleep(1);
    return 0;
}

int LibcBridge::cfsetispeed_wrapper(struct linux_termios *termios_p, unsigned int speed)
{
    if (termios_p)
        termios_p->c_ispeed = speed;
    return 0;
}

int LibcBridge::cfsetospeed_wrapper(struct linux_termios *termios_p, unsigned int speed)
{
    if (termios_p)
        termios_p->c_ospeed = speed;
    return 0;
}

// ========================================================================
// --- Process Management Implementation ---
// ========================================================================

int LibcBridge::kill_wrapper(int pid, int sig)
{
    int my_pid = _getpid();
    log_info("kill(%d, %d) called", pid, sig);

    if (pid == my_pid || pid == 0)
    {
        // Signal 0 is existence check
        if (sig == 0)
            return 0;

        // SIGKILL (9) or SIGTERM (15) -> Terminate self
        if (sig == 9 || sig == 15)
        {
            log_warn("kill: Process requested self-termination");
            exit(0);
        }
    }

    return 0;
}

int LibcBridge::wait_wrapper(int *wstatus)
{
    log_warn("wait() called: No child processes to wait for. Returning ECHILD.");

    Sleep(10);

    if (wstatus)
        *wstatus = 0;
    errno = ECHILD;
    return -1;
}

int LibcBridge::__xmknod_wrapper(int ver, const char *path, unsigned int mode, void *dev)
{
    log_info("__xmknod called: ver=%d, path=\"%s\", mode=0x%x", ver, path, mode);

    return 0;
}
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "LibcBridge.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <io.h>     // _fileno, _isatty, _setmode, _creat, _fstat64
#include <fcntl.h>  // _O_BINARY
#include <direct.h> // _getcwd
#include <sys/stat.h>
#include <sys/utime.h>
#include <csignal>
#include <mutex>

// --- Helper Functions ---

// Linux path (/) to Windows path (\) conversion
static void ConvertPath(char* dst, const char* src, size_t size) {
    if (!src || !dst) return;
    strncpy(dst, src, size);
    dst[size - 1] = '\0';
    for (size_t i = 0; dst[i]; i++) {
        if (dst[i] == '/') dst[i] = '\\';
    }
}

// Network thread safety mutex for non-reentrant Winsock functions
static std::mutex g_net_mutex;

// ========================================================================
// --- File Input/Output (stdio.h / io.h) ---
// ========================================================================

FILE* LibcBridge::fopen_wrapper(const char* filename, const char* mode) {
    char winPath[MAX_PATH];
    ConvertPath(winPath, filename, MAX_PATH);
    return fopen(winPath, mode);
}

FILE* LibcBridge::fopen64_wrapper(const char* filename, const char* mode) {
    return fopen_wrapper(filename, mode);
}

int LibcBridge::fclose_wrapper(FILE* stream) {
    return fclose(stream);
}

int LibcBridge::creat_wrapper(const char* pathname, int mode) {
    char winPath[MAX_PATH];
    ConvertPath(winPath, pathname, MAX_PATH);
    return _creat(winPath, mode);
}

size_t LibcBridge::fread_wrapper(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fread(ptr, size, nmemb, stream);
}

size_t LibcBridge::fwrite_wrapper(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}

int LibcBridge::fseek_wrapper(FILE* stream, long offset, int whence) {
    return fseek(stream, offset, whence);
}

long LibcBridge::ftell_wrapper(FILE* stream) {
    return ftell(stream);
}

int LibcBridge::fseeko64_wrapper(FILE* stream, off64_t offset, int whence) {
    return _fseeki64(stream, offset, whence);
}

off64_t LibcBridge::ftello64_wrapper(FILE* stream) {
    return _ftelli64(stream);
}

void LibcBridge::rewind_wrapper(FILE* stream) {
    rewind(stream);
}

// --- Character & String I/O ---
int LibcBridge::fgetc_wrapper(FILE* stream) {
    return fgetc(stream);
}

char* LibcBridge::fgets_wrapper(char* s, int size, FILE* stream) {
    return fgets(s, size, stream);
}

int LibcBridge::fputs_wrapper(const char* s, FILE* stream) {
    return fputs(s, stream);
}

int LibcBridge::fputc_wrapper(int c, FILE* stream) {
    return fputc(c, stream);
}

int LibcBridge::putc_wrapper(int c, FILE* stream) {
    return putc(c, stream);
}

int LibcBridge::putchar_wrapper(int c) {
    return putchar(c);
}

int LibcBridge::puts_wrapper(const char* s) {
    return puts(s);
}

int LibcBridge::ungetc_wrapper(int c, FILE* stream) {
    return ungetc(c, stream);
}

int LibcBridge::getc_wrapper(FILE* stream) {
    return getc(stream);
}

// --- File Stream Management ---
int LibcBridge::feof_wrapper(FILE* stream) {
    return feof(stream);
}

int LibcBridge::ferror_wrapper(FILE* stream) {
    return ferror(stream);
}

int LibcBridge::fflush_wrapper(FILE* stream) {
    return fflush(stream);
}

int LibcBridge::fileno_wrapper(FILE* stream) {
    return _fileno(stream);
}

FILE* LibcBridge::fdopen_wrapper(int fd, const char* mode) {
    return _fdopen(fd, mode);
}

void LibcBridge::perror_wrapper(const char* s) {
    perror(s);
}

int LibcBridge::setvbuf_wrapper(FILE* stream, char* buf, int mode, size_t size) {
    return setvbuf(stream, buf, mode, size);
}

int LibcBridge::remove_wrapper(const char* pathname) {
    char winPath[MAX_PATH];
    ConvertPath(winPath, pathname, MAX_PATH);
    return remove(winPath);
}

// ========================================================================
// --- Formatted I/O ---
// ========================================================================

int LibcBridge::printf_wrapper(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
}

int LibcBridge::vprintf_wrapper(const char* format, va_list ap) {
    return vprintf(format, ap);
}

int LibcBridge::fprintf_wrapper(FILE* stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfprintf(stream, format, args);
    va_end(args);
    return ret;
}

int LibcBridge::vfprintf_wrapper(FILE* stream, const char* format, va_list ap) {
    return vfprintf(stream, format, ap);
}

int LibcBridge::fscanf_wrapper(FILE* stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfscanf(stream, format, args);
    va_end(args);
    return ret;
}

int LibcBridge::sscanf_wrapper(const char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsscanf(str, format, args);
    va_end(args);
    return ret;
}

int LibcBridge::snprintf_wrapper(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = _vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}

int LibcBridge::vsnprintf_wrapper(char* str, size_t size, const char* format, va_list ap) {
    return _vsnprintf(str, size, format, ap);
}

int LibcBridge::vsprintf_wrapper(char* str, const char* format, va_list ap) {
    return vsprintf(str, format, ap);
}

// ========================================================================
// --- String & Memory Operations (string.h) ---
// ========================================================================

char* LibcBridge::strchr_wrapper(const char* s, int c) {
    return (char*)strchr(s, c);
}

char* LibcBridge::strrchr_wrapper(const char* s, int c) {
    return (char*)strrchr(s, c);
}

char* LibcBridge::strstr_wrapper(const char* haystack, const char* needle) {
    return (char*)strstr(haystack, needle);
}

void* LibcBridge::memchr_wrapper(const void* s, int c, size_t n) {
    return (void*)memchr(s, c, n);
}

char* LibcBridge::strtok_r_wrapper(char* s, const char* delim, char** saveptr) {
    return strtok_s(s, delim, saveptr);
}

char* LibcBridge::strncat_wrapper(char* dest, const char* src, size_t n) {
    return strncat(dest, src, n);
}

char* LibcBridge::strncpy_wrapper(char* dest, const char* src, size_t n) {
    return strncpy(dest, src, n);
}

char* LibcBridge::strerror_wrapper(int errnum) {
    return strerror(errnum);
}

char* LibcBridge::strdup_wrapper(const char* s) {
    if (!s) return nullptr;
    size_t len = strlen(s) + 1;
    void* ptr = _aligned_malloc(len, 16);
    if (ptr) memcpy(ptr, s, len);
    return (char*)ptr;
}

int LibcBridge::strcasecmp_wrapper(const char* s1, const char* s2) {
    return _stricmp(s1, s2);
}

void* LibcBridge::memmove_wrapper(void* dest, const void* src, size_t n) {
    return memmove(dest, src, n);
}

int LibcBridge::strcoll_wrapper(const char* s1, const char* s2) {
    return strcoll(s1, s2);
}

size_t LibcBridge::strxfrm_wrapper(char* dest, const char* src, size_t n) {
    return strxfrm(dest, src, n);
}

// ========================================================================
// --- Path & Directory Management ---
// ========================================================================

char* LibcBridge::getcwd_wrapper(char* buf, size_t size) {
    if (buf != nullptr) {
        return _getcwd(buf, (int)size);
    }
    else {
        char* tmp = _getcwd(NULL, 0);
        if (!tmp) return nullptr;

        size_t len = strlen(tmp) + 1;
        if (size > 0 && size < len) len = size;

        char* aligned_ptr = (char*)_aligned_malloc(len, 16);
        if (aligned_ptr) {
            strncpy(aligned_ptr, tmp, len);
            aligned_ptr[len - 1] = '\0';
        }

        free(tmp);
        return aligned_ptr;
    }
}

char* LibcBridge::realpath_wrapper(const char* path, char* resolved_path) {
    char winPath[MAX_PATH];
    ConvertPath(winPath, path, MAX_PATH);

    if (resolved_path) {
        return _fullpath(resolved_path, winPath, MAX_PATH);
    }
    else {
        char buffer[MAX_PATH];
        if (_fullpath(buffer, winPath, MAX_PATH)) {
            size_t len = strlen(buffer) + 1;
            void* ptr = _aligned_malloc(len, 16);
            if (ptr) {
                memcpy(ptr, buffer, len);
                return (char*)ptr;
            }
        }
    }
    return nullptr;
}

// ========================================================================
// --- Time & Clock Functions (time.h / sys/time.h) ---
// ========================================================================

int LibcBridge::utime_wrapper(const char* filename, const void* times) {
    char winPath[MAX_PATH];
    ConvertPath(winPath, filename, MAX_PATH);
    return _utime(winPath, (struct _utimbuf*)times);
}

size_t LibcBridge::strftime_wrapper(char* s, size_t max, const char* format, const struct tm* tm) {
    return strftime(s, max, format, tm);
}

struct tm* LibcBridge::localtime_wrapper(const time_t* timep) {
    return localtime(timep);
}

time_t LibcBridge::mktime_wrapper(struct tm* tm) {
    return mktime(tm);
}

int LibcBridge::nanosleep_wrapper(const struct timespec* req, struct timespec* rem) {
    if (req) {
        // Convert nanoseconds to milliseconds
        DWORD ms = req->tv_sec * 1000 + (req->tv_nsec / 1000000);
        Sleep(ms);
    }
    return 0;
}

int LibcBridge::settimeofday_wrapper(const struct timeval* tv, const void* tz) {
    if (!tv) return -1;

    // timeval (UNIX epoch) to SYSTEMTIME conversion
    FILETIME ft;
    unsigned __int64 ticks = (unsigned __int64)tv->tv_sec * 10000000ULL +
        (unsigned __int64)tv->tv_usec * 10ULL +
        116444736000000000ULL;
    ft.dwLowDateTime = (DWORD)ticks;
    ft.dwHighDateTime = (DWORD)(ticks >> 32);

    SYSTEMTIME st;
    if (FileTimeToSystemTime(&ft, &st)) {
        return SetSystemTime(&st) ? 0 : -1;
    }
    return -1;
}

// ========================================================================
// --- Standard Library & System (stdlib.h / signal.h) ---
// ========================================================================

int LibcBridge::rand_wrapper() {
    return rand();
}

void LibcBridge::srand_wrapper(unsigned int seed) {
    srand(seed);
}

char* LibcBridge::getenv_wrapper(const char* name) {
    return getenv(name);
}

int LibcBridge::system_wrapper(const char* command) {
    return system(command);
}

void LibcBridge::abort_wrapper() {
    abort();
}

int LibcBridge::raise_wrapper(int sig) {
    return raise(sig);
}

void LibcBridge::_exit_wrapper(int status) {
    _exit(status);
}

// Simulation of standard Linux 32-bit stat structure (common in arcade binaries)
struct linux_stat {
    uint32_t st_dev; uint16_t __pad1; uint32_t st_ino; uint32_t st_mode;
    uint32_t st_nlink; uint32_t st_uid; uint32_t st_gid; uint32_t st_rdev;
    uint16_t __pad2; uint32_t st_size; uint32_t st_blksize; uint32_t st_blocks;
    uint32_t st_atime; uint32_t st_atime_nsec; uint32_t st_mtime;
    uint32_t st_mtime_nsec; uint32_t st_ctime; uint32_t st_ctime_nsec;
    uint32_t __unused4; uint32_t __unused5;
};

int LibcBridge::fxstat_wrapper(int ver, int fd, void* stat_buf) {
    struct _stat64 st;
    if (_fstat64(fd, &st) != 0) return -1;

    // Manual field-by-field copy from Windows _stat64 to Linux-like structure
    struct linux_stat* ls = (struct linux_stat*)stat_buf;
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
// --- Network & Socket Utilities (arpa/inet.h / netdb.h) ---
// ========================================================================

int LibcBridge::inet_aton_wrapper(const char* cp, struct in_addr* inp) {
    unsigned long addr = inet_addr(cp);
    if (addr == INADDR_NONE && strcmp(cp, "255.255.255.255") != 0) return 0;
    if (inp) inp->s_addr = addr;
    return 1;
}

int LibcBridge::gethostbyname_r_wrapper(const char* name, void* ret, char* buf, size_t buflen, void** result, int* h_errnop) {
    std::lock_guard<std::mutex> lock(g_net_mutex);
    struct hostent* he = gethostbyname(name);
    if (!he) {
        if (h_errnop) *h_errnop = h_errno;
        *result = nullptr;
        return -1;
    }
    // Deep copy into the provided result buffer
    memcpy(ret, he, sizeof(struct hostent));
    *result = ret;
    return 0;
}

int LibcBridge::gethostbyaddr_r_wrapper(const void* addr, int len, int type, void* ret, char* buf, size_t buflen, void** result, int* h_errnop) {
    std::lock_guard<std::mutex> lock(g_net_mutex);
    struct hostent* he = gethostbyaddr((const char*)addr, len, type);
    if (!he) {
        if (h_errnop) *h_errnop = h_errno;
        *result = nullptr;
        return -1;
    }
    // Deep copy into the provided result buffer
    memcpy(ret, he, sizeof(struct hostent));
    *result = ret;
    return 0;
}

// ========================================================================
// --- Locale & Wide Character Support (locale.h / wchar.h / wctype.h) ---
// ========================================================================

char* LibcBridge::setlocale_wrapper(int category, const char* locale) {
    return setlocale(category, locale);
}

wint_t LibcBridge::btowc_wrapper(int c) {
    return btowc(c);
}

int LibcBridge::wctob_wrapper(wint_t c) {
    return wctob(c);
}

size_t LibcBridge::mbrtowc_wrapper(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps) {
    return mbrtowc(pwc, s, n, ps);
}

size_t LibcBridge::wcrtomb_wrapper(char* s, wchar_t wc, mbstate_t* ps) {
    return wcrtomb(s, wc, ps);
}

wint_t LibcBridge::getwc_wrapper(FILE* stream) {
    return getwc(stream);
}

wint_t LibcBridge::putwc_wrapper(wchar_t wc, FILE* stream) {
    return putwc(wc, stream);
}

wint_t LibcBridge::ungetwc_wrapper(wint_t wc, FILE* stream) {
    return ungetwc(wc, stream);
}

size_t LibcBridge::wcslen_wrapper(const wchar_t* s) {
    return wcslen(s);
}

wchar_t* LibcBridge::wmemcpy_wrapper(wchar_t* dest, const wchar_t* src, size_t n) {
    return wmemcpy(dest, src, n);
}

wchar_t* LibcBridge::wmemmove_wrapper(wchar_t* dest, const wchar_t* src, size_t n) {
    return wmemmove(dest, src, n);
}

wchar_t* LibcBridge::wmemset_wrapper(wchar_t* wcs, wchar_t wc, size_t n) {
    return wmemset(wcs, wc, n);
}

wchar_t* LibcBridge::wmemchr_wrapper(const wchar_t* wcs, wchar_t wc, size_t n) {
    return (wchar_t*)wmemchr(wcs, wc, n);
}

int LibcBridge::wmemcmp_wrapper(const wchar_t* s1, const wchar_t* s2, size_t n) {
    return wmemcmp(s1, s2, n);
}

int LibcBridge::wcscoll_wrapper(const wchar_t* s1, const wchar_t* s2) {
    return wcscoll(s1, s2);
}

size_t LibcBridge::wcsxfrm_wrapper(wchar_t* dest, const wchar_t* src, size_t n) {
    return wcsxfrm(dest, src, n);
}

size_t LibcBridge::wcsftime_wrapper(wchar_t* wcs, size_t maxsize, const wchar_t* format, const struct tm* tm) {
    return wcsftime(wcs, maxsize, format, tm);
}

int LibcBridge::tolower_wrapper(int c) {
    return tolower(c);
}

int LibcBridge::toupper_wrapper(int c) {
    return toupper(c);
}

wint_t LibcBridge::towlower_wrapper(wint_t wc) {
    return towlower(wc);
}

wint_t LibcBridge::towupper_wrapper(wint_t wc) {
    return towupper(wc);
}

wctype_t LibcBridge::wctype_wrapper(const char* property) {
    if (strcmp(property, "alnum") == 0) return _ALPHA | _DIGIT;
    if (strcmp(property, "alpha") == 0) return _ALPHA;
    if (strcmp(property, "cntrl") == 0) return _CONTROL;
    if (strcmp(property, "digit") == 0) return _DIGIT;
    if (strcmp(property, "graph") == 0) return _PUNCT | _ALPHA | _DIGIT;
    if (strcmp(property, "lower") == 0) return _LOWER;
    if (strcmp(property, "print") == 0) return _BLANK | _PUNCT | _ALPHA | _DIGIT;
    if (strcmp(property, "punct") == 0) return _PUNCT;
    if (strcmp(property, "space") == 0) return _SPACE;
    if (strcmp(property, "upper") == 0) return _UPPER;
    if (strcmp(property, "xdigit") == 0) return _HEX;
    return 0;
}

int LibcBridge::iswctype_wrapper(wint_t wc, wctype_t desc) {
    return iswctype(wc, desc);
}
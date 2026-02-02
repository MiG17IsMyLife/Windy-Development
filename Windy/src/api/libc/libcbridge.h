#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
// lazy fix for pthreads stuffs
#ifndef HAVE_STRUCT_TIMESPEC
#define HAVE_STRUCT_TIMESPEC
#endif

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdarg>
#include <cwchar>
#include <cwctype>
#include <cstdint>
#include <sys/types.h>
#include <winsock2.h>

// Linux off64_t definition
typedef int64_t off64_t;

class LibcBridge {
public:
    // ========================================================================
    // --- File Input/Output (stdio.h / io.h) ---
    // ========================================================================
    static FILE* fopen_wrapper(const char* filename, const char* mode);
    static FILE* fopen64_wrapper(const char* filename, const char* mode);
    static int   fclose_wrapper(FILE* stream);
    static int   creat_wrapper(const char* pathname, int mode);
    static size_t fread_wrapper(void* ptr, size_t size, size_t nmemb, FILE* stream);
    static size_t fwrite_wrapper(const void* ptr, size_t size, size_t nmemb, FILE* stream);
    static int   fseek_wrapper(FILE* stream, long offset, int whence);
    static long  ftell_wrapper(FILE* stream);
    static int   fseeko64_wrapper(FILE* stream, off64_t offset, int whence);
    static off64_t ftello64_wrapper(FILE* stream);
    static void  rewind_wrapper(FILE* stream);

    // --- Character & String I/O ---
    static int   fgetc_wrapper(FILE* stream);
    static char* fgets_wrapper(char* s, int size, FILE* stream);
    static int   fputs_wrapper(const char* s, FILE* stream);
    static int   fputc_wrapper(int c, FILE* stream);
    static int   putc_wrapper(int c, FILE* stream);
    static int   putchar_wrapper(int c);
    static int   puts_wrapper(const char* s);
    static int   ungetc_wrapper(int c, FILE* stream);
    static int   getc_wrapper(FILE* stream);

    // --- File Stream Management ---
    static int   feof_wrapper(FILE* stream);
    static int   ferror_wrapper(FILE* stream);
    static int   fflush_wrapper(FILE* stream);
    static int   fileno_wrapper(FILE* stream);
    static FILE* fdopen_wrapper(int fd, const char* mode);
    static void  perror_wrapper(const char* s);
    static int   setvbuf_wrapper(FILE* stream, char* buf, int mode, size_t size);
    static int   remove_wrapper(const char* pathname);

    // ========================================================================
    // --- Formatted I/O (printf / scanf family) ---
    // ========================================================================
    static int printf_wrapper(const char* format, ...);
    static int vprintf_wrapper(const char* format, va_list ap);
    static int fprintf_wrapper(FILE* stream, const char* format, ...);
    static int vfprintf_wrapper(FILE* stream, const char* format, va_list ap);
    static int fscanf_wrapper(FILE* stream, const char* format, ...);
    static int sscanf_wrapper(const char* str, const char* format, ...);
    static int snprintf_wrapper(char* str, size_t size, const char* format, ...);
    static int vsnprintf_wrapper(char* str, size_t size, const char* format, va_list ap);
    static int vsprintf_wrapper(char* str, const char* format, va_list ap);

    static int sprintf_wrapper(char* str, const char* format, ...);

    // ========================================================================
    // --- String & Memory Operations (string.h) ---
    // ========================================================================
    static char* strchr_wrapper(const char* s, int c);
    static char* strrchr_wrapper(const char* s, int c);
    static char* strstr_wrapper(const char* haystack, const char* needle);
    static void* memchr_wrapper(const void* s, int c, size_t n);
    static void* memmove_wrapper(void* dest, const void* src, size_t n);
    static char* strtok_r_wrapper(char* s, const char* delim, char** saveptr);
    static char* strncat_wrapper(char* dest, const char* src, size_t n);
    static char* strncpy_wrapper(char* dest, const char* src, size_t n);
    static char* strdup_wrapper(const char* s);
    static int   strcasecmp_wrapper(const char* s1, const char* s2);
    static int   strcoll_wrapper(const char* s1, const char* s2);
    static size_t strxfrm_wrapper(char* dest, const char* src, size_t n);
    static char* strerror_wrapper(int errnum);

    static void* malloc_wrapper(size_t size);
    static void  free_wrapper(void* ptr);
    static void* calloc_wrapper(size_t nmemb, size_t size);
    static void* realloc_wrapper(void* ptr, size_t size);
    static void* memalign_wrapper(size_t alignment, size_t size);

    // ========================================================================
    // --- Path & Directory Management ---
    // ========================================================================
    static char* getcwd_wrapper(char* buf, size_t size);
    static char* realpath_wrapper(const char* path, char* resolved_path);

    // ========================================================================
    // --- Time & Clock Functions (time.h / sys/time.h) ---
    // ========================================================================
    static time_t mktime_wrapper(struct tm* tm);
    static struct tm* localtime_wrapper(const time_t* timep);
    static size_t strftime_wrapper(char* s, size_t max, const char* format, const struct tm* tm);
    static int   utime_wrapper(const char* filename, const void* times);
    static int   nanosleep_wrapper(const struct timespec* req, struct timespec* rem);
    static int   settimeofday_wrapper(const struct timeval* tv, const void* tz);

    static int   gettimeofday_wrapper(struct timeval* tv, void* tz);
    static struct tm* localtime_r_wrapper(const time_t* timep, struct tm* result);
    static int   usleep_wrapper(unsigned int usec);
    static unsigned int sleep_wrapper(unsigned int seconds);

    // ========================================================================
    // --- Standard Library & System (stdlib.h / signal.h) ---
    // ========================================================================
    static int   rand_wrapper();
    static void  srand_wrapper(unsigned int seed);
    static char* getenv_wrapper(const char* name);
    static int   system_wrapper(const char* command);
    static void  abort_wrapper();
    static int   raise_wrapper(int sig);
    static void  _exit_wrapper(int status);

    static void  exit_wrapper(int status);

    static int   fxstat_wrapper(int ver, int fd, void* stat_buf); // System stat (fstat)

    // ========================================================================
    // --- Network & Socket Utilities (arpa/inet.h / netdb.h) ---
    // ========================================================================
    static int inet_aton_wrapper(const char* cp, struct in_addr* inp);
    static int gethostbyname_r_wrapper(const char* name, void* ret, char* buf, size_t buflen, void** result, int* h_errnop);
    static int gethostbyaddr_r_wrapper(const void* addr, int len, int type, void* ret, char* buf, size_t buflen, void** result, int* h_errnop);

    // ========================================================================
    // --- Locale & Wide Character Support (locale.h / wchar.h / wctype.h) ---
    // ========================================================================
    static char* setlocale_wrapper(int category, const char* locale);

    // --- Multi-byte & Wide-char Conversions ---
    static wint_t btowc_wrapper(int c);
    static int    wctob_wrapper(wint_t c);
    static size_t mbrtowc_wrapper(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps);
    static size_t wcrtomb_wrapper(char* s, wchar_t wc, mbstate_t* ps);

    // --- Wide Character I/O ---
    static wint_t getwc_wrapper(FILE* stream);
    static wint_t putwc_wrapper(wchar_t wc, FILE* stream);
    static wint_t ungetwc_wrapper(wint_t wc, FILE* stream);

    // --- Wide String & Memory ---
    static size_t   wcslen_wrapper(const wchar_t* s);
    static wchar_t* wmemcpy_wrapper(wchar_t* dest, const wchar_t* src, size_t n);
    static wchar_t* wmemmove_wrapper(wchar_t* dest, const wchar_t* src, size_t n);
    static wchar_t* wmemset_wrapper(wchar_t* wcs, wchar_t wc, size_t n);
    static wchar_t* wmemchr_wrapper(const wchar_t* wcs, wchar_t wc, size_t n);
    static int      wmemcmp_wrapper(const wchar_t* s1, const wchar_t* s2, size_t n);
    static int      wcscoll_wrapper(const wchar_t* s1, const wchar_t* s2);
    static size_t   wcsxfrm_wrapper(wchar_t* dest, const wchar_t* src, size_t n);
    static size_t   wcsftime_wrapper(wchar_t* wcs, size_t maxsize, const wchar_t* format, const struct tm* tm);

    // --- Character Case & Type ---
    static int    tolower_wrapper(int c);
    static int    toupper_wrapper(int c);
    static wint_t towlower_wrapper(wint_t wc);
    static wint_t towupper_wrapper(wint_t wc);
    static wctype_t wctype_wrapper(const char* property);
    static int    iswctype_wrapper(wint_t wc, wctype_t desc);

    // ========================================================================
    // --- Termios (Serial Port) Support (Added) ---
    // ========================================================================
    struct linux_termios {
        unsigned int c_iflag; // input mode flags
        unsigned int c_oflag; // output mode flags
        unsigned int c_cflag; // control mode flags
        unsigned int c_lflag; // local mode flags
        unsigned char c_line; // line discipline
        unsigned char c_cc[32]; // control characters
        unsigned int c_ispeed; // input speed
        unsigned int c_ospeed; // output speed
    };

    static int inet_pton_wrapper(int af, const char* src, void* dst);
    static int rand_r_wrapper(unsigned int* seedp);
    static char* ctime_r_wrapper(const time_t* timep, char* buf);

    static int tcgetattr_wrapper(int fd, struct linux_termios* termios_p);
    static int tcsetattr_wrapper(int fd, int optional_actions, const struct linux_termios* termios_p);
    static int tcflush_wrapper(int fd, int queue_selector);
    static int tcdrain_wrapper(int fd);
    static int cfsetispeed_wrapper(struct linux_termios* termios_p, unsigned int speed);
    static int cfsetospeed_wrapper(struct linux_termios* termios_p, unsigned int speed);

    // ========================================================================
    // --- Process Management ---
    // ========================================================================
    static int kill_wrapper(int pid, int sig);

    static int wait_wrapper(int* wstatus);
    static int __xmknod_wrapper(int ver, const char* path, unsigned int mode, void* dev);
};
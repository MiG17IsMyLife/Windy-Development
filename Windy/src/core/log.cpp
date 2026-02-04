#define _CRT_SECURE_NO_WARNINGS

#include "log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ============================================================
// Configuration
// ============================================================

// Control appearance (1 = enabled, 0 = disabled)
#define LOG_COLOR_ENABLED 0
#define LOG_TIME_ENABLED 0
#define LOG_FILE_INFO_ENABLED 1

// When enabled (1), it counts repeat message instead of spamming
#define LOG_NO_REPEAT 1
#define LOG_REPEAT_BUFFER_SIZE 8192

// ============================================================
// Global State
// ============================================================

// Minimum level to output logs (can be changed at runtime)
static int g_logMinLevel = LOG_TRACE;

// Show file/line info
static int g_logShowFileInfo = 1;

// Windows high-resolution timer
#ifdef _WIN32
static LARGE_INTEGER logStartTime = { 0 };
static LARGE_INTEGER logFrequency = { 0 };
static int logTimerInitialized = 0;
#else
static struct timespec logStartTime = { 0, 0 };
#endif

// Hold a counter of message repeat
static unsigned int logRepeatCount = 0;
static char logLastMessage[LOG_REPEAT_BUFFER_SIZE] = { 0 };

// ANSI color codes
static const char* log_style[] = {
    "\x1b[90m",  // TRACE : Gray
    "\x1b[36m",  // DEBUG : Cyan
    "\x1b[0m",   // GAME  : White/Default
    "\x1b[32m",  // INFO  : Green
    "\x1b[33m",  // WARN  : Yellow
    "\x1b[31m",  // ERROR : Red
    "\x1b[95m",  // FATAL : Bright Magenta
};

static const char* log_names[] = {
    "TRACE", "DEBUG", "GAME ", "INFO ", "WARN ", "ERROR", "FATAL"
};

// ============================================================
// Initialization
// ============================================================

void logEnableWindowsAnsi() {
#ifdef _WIN32
    // Enable ANSI escape sequences on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hErr, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hErr, dwMode);
        }
    }
#endif
}

void logInitTimer() {
#ifdef _WIN32
    QueryPerformanceFrequency(&logFrequency);
    QueryPerformanceCounter(&logStartTime);
    logTimerInitialized = 1;
#else
    clock_gettime(CLOCK_MONOTONIC, &logStartTime);
#endif
}

void logInit() {
    logEnableWindowsAnsi();
    logInitTimer();
}

void logSetMinLevel(int level) {
    if (level >= LOG_TRACE && level <= LOG_FATAL) {
        g_logMinLevel = level;
    }
}

void logSetShowFileInfo(int show) {
    g_logShowFileInfo = show;
}

// ============================================================
// Timer Functions
// ============================================================

void logGetElapsedTime(long* seconds, long* milliseconds) {
#ifdef _WIN32
    if (!logTimerInitialized) {
        logInitTimer();
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    LONGLONG elapsed = now.QuadPart - logStartTime.QuadPart;
    LONGLONG elapsedMs = (elapsed * 1000) / logFrequency.QuadPart;

    *seconds = (long)(elapsedMs / 1000);
    *milliseconds = (long)(elapsedMs % 1000);
#else
    struct timespec now;
    if (logStartTime.tv_sec == 0) {
        logInitTimer();
    }
    clock_gettime(CLOCK_MONOTONIC, &now);

    *seconds = now.tv_sec - logStartTime.tv_sec;
    *milliseconds = (now.tv_nsec - logStartTime.tv_nsec) / 1000000;

    if (*milliseconds < 0) {
        *milliseconds += 1000;
        (*seconds)--;
    }
#endif
}

// ============================================================
// Core Logging Functions
// ============================================================

int logSanityChecks(int level, const char* message) {
    if (level < LOG_TRACE || level > LOG_FATAL) {
        return -1;
    }

    if (level < g_logMinLevel) {
        return 0;
    }

    if (!message) {
        return -1;
    }

    return 1;
}

FILE* logGetStream(int level) {
    // Send errors and fatals to stderr
    if (level >= LOG_ERROR) {
        return stderr;
    }
    return stdout;
}

LogFormattedMessage logFormatMessage(const char* message, va_list args) {
    LogFormattedMessage result = { NULL, 0, 0, 0 };

    if (!message) {
        return result;
    }

    va_list args_copy;
    va_copy(args_copy, args);

    int size = vsnprintf(NULL, 0, message, args_copy);
    va_end(args_copy);

    if (size < 0) {
        return result;
    }

    char* formatted_message = (char*)malloc((size_t)size + 1);
    if (!formatted_message) {
        return result;
    }

    vsnprintf(formatted_message, (size_t)size + 1, message, args);

    // Check for newline at end
    int ends_with_newline = 0;
    if (size > 0 && (formatted_message[size - 1] == '\n' || formatted_message[size - 1] == '\r')) {
        ends_with_newline = 1;
        // Remove trailing newline for cleaner output
        formatted_message[size - 1] = '\0';
        size--;
    }

    // Check for repeat messages
#if LOG_NO_REPEAT
    if (((size_t)size + 1) < LOG_REPEAT_BUFFER_SIZE) {
        if (strcmp(logLastMessage, formatted_message) == 0) {
            logRepeatCount++;
        }
        else {
            strncpy(logLastMessage, formatted_message, (size_t)size + 1);
            logRepeatCount = 0;
        }
    }
    else {
        logRepeatCount = 0;
    }
#endif

    result.message = formatted_message;
    result.size = (size_t)size;
    result.endWithNewLine = ends_with_newline;
    result.repeat = logRepeatCount;

    return result;
}

int logPrintHeader(FILE* stream, int level, const char* file, int line) {
    // Color
#if LOG_COLOR_ENABLED
    fprintf(stream, "%s", log_style[level]);
#endif

    // Timestamp
#if LOG_TIME_ENABLED
    long seconds, milliseconds;
    logGetElapsedTime(&seconds, &milliseconds);
    fprintf(stream, "[%04ld.%03ld] ", seconds, milliseconds);
#endif

    // Log level
    fprintf(stream, "%s ", log_names[level]);

    // File and line info
#if LOG_FILE_INFO_ENABLED
    if (g_logShowFileInfo && file) {
        // Extract just the filename from the path
        const char* filename = file;
        const char* p = file;
        while (*p) {
            if (*p == '/' || *p == '\\') {
                filename = p + 1;
            }
            p++;
        }
        fprintf(stream, "[%s:%d] ", filename, line);
    }
#endif

    // Reset color before message
#if LOG_COLOR_ENABLED
    fprintf(stream, "\x1b[0m");
#endif

    return 0;
}

int logPrintMessage(FILE* stream, LogFormattedMessage formattedMessage, int level, const char* file, int line) {
    int ret;

#if LOG_NO_REPEAT
    if (formattedMessage.repeat > 0) {
        // Clear line and reprint with repeat count
        fprintf(stream, "\r\x1b[K");
        logPrintHeader(stream, level, file, line);
        ret = fprintf(stream, "%s (x%u)\n", formattedMessage.message, formattedMessage.repeat + 1);
    }
    else {
        ret = fprintf(stream, "%s\n", formattedMessage.message);
    }
#else
    ret = fprintf(stream, "%s\n", formattedMessage.message);
#endif

    fflush(stream);
    return ret;
}

int logVA(int level, const char* file, int line, const char* message, va_list args) {
    int ret = logSanityChecks(level, message);
    if (ret < 1) {
        return ret;
    }

    LogFormattedMessage formattedMessage = logFormatMessage(message, args);
    if (!formattedMessage.message) {
        return -1;
    }

    FILE* stream = logGetStream(level);
    logPrintHeader(stream, level, file, line);
    ret = logPrintMessage(stream, formattedMessage, level, file, line);

    free(formattedMessage.message);
    return ret;
}

int logGeneric(int level, const char* file, int line, const char* message, ...) {
    int ret = logSanityChecks(level, message);
    if (ret < 1) {
        return ret;
    }

    va_list args;
    va_start(args, message);
    LogFormattedMessage formattedMessage = logFormatMessage(message, args);
    va_end(args);

    if (!formattedMessage.message) {
        return -1;
    }

    FILE* stream = logGetStream(level);
    logPrintHeader(stream, level, file, line);
    ret = logPrintMessage(stream, formattedMessage, level, file, line);

    free(formattedMessage.message);
    return ret;
}
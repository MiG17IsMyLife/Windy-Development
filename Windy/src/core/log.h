#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

    // Structure to hold the formatted message and its size
    typedef struct {
        char* message;          // The formatted message
        size_t size;            // The size of the formatted message
        int endWithNewLine;     // 1 if it ends with a new line
        unsigned int repeat;    // How many times the message was repeated
    } LogFormattedMessage;

    // Log levels
    enum {
        LOG_TRACE = 0,  // Detailed trace (symbol resolution, etc.)
        LOG_DEBUG = 1,  // Debug info (init complete, state changes)
        LOG_GAME = 2,  // Game output (original printf from game)
        LOG_INFO = 3,  // Important info (startup, load success)
        LOG_WARN = 4,  // Warnings (unimplemented, stubs)
        LOG_ERROR = 5,  // Errors (failures)
        LOG_FATAL = 6   // Fatal errors (cannot continue)
    };

    // Logging macros - automatically include file and line
#define log_trace(...) logGeneric(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) logGeneric(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_game(...)  logGeneric(LOG_GAME,  __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  logGeneric(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  logGeneric(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) logGeneric(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) logGeneric(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

// VA list versions
#define logVA_game(...)  logVA(LOG_GAME,  __FILE__, __LINE__, __VA_ARGS__)
#define logVA_trace(...) logVA(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define logVA_debug(...) logVA(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define logVA_info(...)  logVA(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define logVA_warn(...)  logVA(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define logVA_error(...) logVA(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

// Function prototypes
    int logGeneric(int level, const char* file, int line, const char* message, ...);
    int logVA(int level, const char* file, int line, const char* message, va_list args);
    int logSanityChecks(int level, const char* message);
    int logPrintHeader(FILE* stream, int level, const char* file, int line);
    int logPrintMessage(FILE* stream, LogFormattedMessage formattedMessage, int level, const char* file, int line);
    LogFormattedMessage logFormatMessage(const char* message, va_list args);
    FILE* logGetStream(int level);
    void logInit();
    void logInitTimer();
    void logGetElapsedTime(long* seconds, long* milliseconds);
    void logEnableWindowsAnsi();
    void logSetMinLevel(int level);
    void logSetShowFileInfo(int show);

#ifdef __cplusplus
}
#endif
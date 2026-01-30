#define _CRT_SECURE_NO_WARNINGS

#include "baseboard.h"
#include "jvsboard.h"
#include "../core/log.h"
#include <cstring>
#include <cstdio>

// IOCTL commands (from Lindbergh HW)
#define BASEBOARD_IOCTL_GET_VERSION     0x4C00
#define BASEBOARD_IOCTL_SET_GAMEINFO    0x4C01
#define BASEBOARD_IOCTL_JVS_EXCHANGE    0x4C02
#define BASEBOARD_IOCTL_SRAM_READ       0x4C03
#define BASEBOARD_IOCTL_SRAM_WRITE      0x4C04
#define BASEBOARD_IOCTL_GET_SERIAL      0x4C05

// Additional IOCTL commands (from games)
#define BC_IOCTL_INIT                   0x0300
#define BC_IOCTL_READ_ID                0x0400      // Read board ID
#define BC_IOCTL_CHECKGAME              0x0600
#define BC_IOCTL_GET_JVS_STATUS         0xBC0A      // JVS status
#define BC_IOCTL_JVS_EXCHANGE           0xBC0B      // JVS packet exchange
#define BC_IOCTL_GET_FIRMWARE_VERSION   0x8004BC02
#define BC_IOCTL_JVS_INIT               0xC020BC06  // JVS initialization

BaseBoard::BaseBoard()
    : m_jvsBoard(nullptr)
    , m_sramSize(sizeof(m_sram))
    , m_jvsResponseLen(0)
{
    memset(m_sram, 0, sizeof(m_sram));
    memset(m_jvsResponse, 0, sizeof(m_jvsResponse));
}

BaseBoard::~BaseBoard() {
    Close();
}

bool BaseBoard::Open() {
    log_info("BaseBoard: Opening...");

    // Load SRAM from file if exists
    if (!m_sramPath.empty()) {
        FILE* f = fopen(m_sramPath.c_str(), "rb");
        if (f) {
            size_t read = fread(m_sram, 1, sizeof(m_sram), f);
            fclose(f);
            log_info("BaseBoard: Loaded SRAM from %s (%zu bytes)", m_sramPath.c_str(), read);
        }
    }

    return true;
}

void BaseBoard::Close() {
    // Save SRAM to file
    if (!m_sramPath.empty()) {
        FILE* f = fopen(m_sramPath.c_str(), "wb");
        if (f) {
            fwrite(m_sram, 1, sizeof(m_sram), f);
            fclose(f);
            log_info("BaseBoard: Saved SRAM to %s", m_sramPath.c_str());
        }
    }
}

int BaseBoard::Read(char* buf, size_t count) {
    // JVS response read
    // Return the pending JVS response if available
    if (m_jvsBoard && buf && count > 0) {
        if (m_jvsResponseLen > 0) {
            size_t copyLen = (count < (size_t)m_jvsResponseLen) ? count : (size_t)m_jvsResponseLen;
            memcpy(buf, m_jvsResponse, copyLen);
            m_jvsResponseLen = 0;  // Clear after reading
            return (int)copyLen;
        }
    }
    return 0;
}

int BaseBoard::Write(const char* buf, size_t count) {
    // JVS command write
    // Process JVS packet and store response
    if (m_jvsBoard && buf && count > 0) {
        int responseLen = 0;
        m_jvsBoard->ProcessPacket((const uint8_t*)buf, (unsigned int)count,
            m_jvsResponse, &responseLen);
        m_jvsResponseLen = responseLen;
        log_trace("BaseBoard: JVS write %zu bytes, response %d bytes", count, responseLen);
    }
    return (int)count;
}

int BaseBoard::Ioctl(unsigned int request, void* data) {
    switch (request) {
    case BC_IOCTL_INIT:
    {
        // BaseBoard initialization - just return success
        log_debug("BaseBoard: BC_IOCTL_INIT");
        return 0;
    }

    case BC_IOCTL_CHECKGAME:
    {
        // Game check - return success to allow game to proceed
        log_debug("BaseBoard: BC_IOCTL_CHECKGAME - returning success");
        return 0;
    }

    case BC_IOCTL_GET_FIRMWARE_VERSION:
    {
        // Return firmware version
        log_debug("BaseBoard: BC_IOCTL_GET_FIRMWARE_VERSION");
        if (data) {
            uint32_t* version = (uint32_t*)data;
            *version = 0x0203;  // Version 2.03
        }
        return 0;
    }

    case BC_IOCTL_GET_JVS_STATUS:
    {
        // JVS status - return ready status
        // This is called frequently by games to poll JVS state
        // log_trace("BaseBoard: BC_IOCTL_GET_JVS_STATUS");  // Too noisy
        if (data) {
            // Return a status structure indicating JVS is ready
            uint32_t* status = (uint32_t*)data;
            *status = 0x01;  // Ready / OK
        }
        return 0;
    }

    case BC_IOCTL_READ_ID:
    {
        // Read board ID - return success
        log_debug("BaseBoard: BC_IOCTL_READ_ID");
        return 0;
    }

    case BC_IOCTL_JVS_EXCHANGE:
    {
        // JVS packet exchange (0xBC0B)
        // This IOCTL triggers JVS communication
        // The actual data is transferred via read/write operations, not through this IOCTL
        // This IOCTL just signals that JVS exchange should occur
        // 
        // The game sends JVS commands via write() and receives responses via read()
        // This IOCTL may just be a synchronization signal
        //
        // For safety, don't try to interpret the data parameter at all
        // Just return success to indicate JVS is ready

        // log_trace("BaseBoard: BC_IOCTL_JVS_EXCHANGE");  // Too noisy
        return 0;
    }

    case BC_IOCTL_JVS_INIT:
    {
        // JVS initialization (0xC020BC06)
        log_debug("BaseBoard: BC_IOCTL_JVS_INIT");
        // This is typically used to initialize JVS communication
        // Return success to indicate JVS is ready
        return 0;
    }

    case BASEBOARD_IOCTL_GET_VERSION:
    {
        // Return baseboard version
        if (data) {
            uint32_t* version = (uint32_t*)data;
            *version = 0x00010000;  // Version 1.0
        }
        return 0;
    }

    case BASEBOARD_IOCTL_JVS_EXCHANGE:
    {
        // JVS packet exchange
        if (!m_jvsBoard || !data) return -1;

        struct JvsExchange {
            uint8_t* input;
            size_t inputSize;
            uint8_t* output;
            int* outputSize;
        };

        JvsExchange* ex = (JvsExchange*)data;
        m_jvsBoard->ProcessPacket(ex->input, ex->inputSize, ex->output, ex->outputSize);
        return 0;
    }

    case BASEBOARD_IOCTL_SRAM_READ:
    {
        // SRAM read
        struct SramAccess {
            uint32_t offset;
            uint32_t size;
            uint8_t* data;
        };

        if (!data) return -1;
        SramAccess* acc = (SramAccess*)data;

        if (acc->offset + acc->size > sizeof(m_sram)) {
            return -1;
        }

        memcpy(acc->data, m_sram + acc->offset, acc->size);
        return 0;
    }

    case BASEBOARD_IOCTL_SRAM_WRITE:
    {
        // SRAM write
        struct SramAccess {
            uint32_t offset;
            uint32_t size;
            uint8_t* data;
        };

        if (!data) return -1;
        SramAccess* acc = (SramAccess*)data;

        if (acc->offset + acc->size > sizeof(m_sram)) {
            return -1;
        }

        memcpy(m_sram + acc->offset, acc->data, acc->size);
        return 0;
    }

    case BASEBOARD_IOCTL_GET_SERIAL:
    {
        // Return serial number
        if (data) {
            char* serial = (char*)data;
            strcpy(serial, "AAA-01A12345");  // Dummy serial
        }
        return 0;
    }

    default:
        log_warn("BaseBoard: Unknown IOCTL 0x%04X", request);
        return -1;
    }
}

void BaseBoard::SetSramPath(const char* path) {
    if (path) {
        m_sramPath = path;
    }
    else {
        m_sramPath.clear();
    }
}
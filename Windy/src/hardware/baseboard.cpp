#define _CRT_SECURE_NO_WARNINGS

#include "baseboard.h"
#include "jvsboard.h"
#include "../core/log.h"
#include <cstring>
#include <cstdio>

// IOCTL commands
#define BASEBOARD_IOCTL_GET_VERSION     0x4C00
#define BASEBOARD_IOCTL_SET_GAMEINFO    0x4C01
#define BASEBOARD_IOCTL_JVS_EXCHANGE    0x4C02
#define BASEBOARD_IOCTL_SRAM_READ       0x4C03
#define BASEBOARD_IOCTL_SRAM_WRITE      0x4C04
#define BASEBOARD_IOCTL_GET_SERIAL      0x4C05

BaseBoard::BaseBoard()
    : m_jvsBoard(nullptr)
    , m_sramSize(sizeof(m_sram))
{
    memset(m_sram, 0, sizeof(m_sram));
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
    // Not typically used for baseboard
    (void)buf;
    (void)count;
    return 0;
}

int BaseBoard::Write(const char* buf, size_t count) {
    // Not typically used for baseboard
    (void)buf;
    (void)count;
    return (int)count;
}

int BaseBoard::Ioctl(unsigned int request, void* data) {
    switch (request) {
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
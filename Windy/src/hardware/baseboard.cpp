#define _CRT_SECURE_NO_WARNINGS

#include "BaseBoard.h"
#include "../src/core/log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// IOCTL Definitions
#define BASEBOARD_INIT          0x300
#define BASEBOARD_GET_VERSION   0x8004BC02
#define BASEBOARD_SEEK_SHM      0x400
#define BASEBOARD_WRITE_SRAM    0x600
#define BASEBOARD_READ_SRAM     0x601
#define BASEBOARD_REQUEST       0xC020BC06
#define BASEBOARD_RECEIVE       0xC020BC07
#define BASEBOARD_READY         0x201

// Request IDs
#define BASEBOARD_GET_SERIAL        0x120
#define BASEBOARD_WRITE_FLASH       0x180
#define BASEBOARD_GET_SENSE_LINE    0x210
#define BASEBOARD_PROCESS_JVS       0x220

#define SERIAL_STRING "FE11-X018012022X"

BaseBoard::BaseBoard() {
    memset(m_sharedMemory, 0, sizeof(m_sharedMemory));
    memset(&m_jvsCommand, 0, sizeof(m_jvsCommand));
    memset(&m_serialCommand, 0, sizeof(m_serialCommand));
    memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
}

BaseBoard::~BaseBoard() {
    Close();
}

bool BaseBoard::Open() {
    log_info("BaseBoard::Open()");

    // lindbergh-loader logic: Ensure file exists, then open rb+
    m_sramFile = fopen(m_sramPath, "a"); // Append mode creates if not exists
    if (m_sramFile) {
        fclose(m_sramFile);
    }
    else {
        log_error("BaseBoard: Cannot open/create %s", m_sramPath);
        return false;
    }

    m_sramFile = fopen(m_sramPath, "rb+");
    if (!m_sramFile) {
        log_error("BaseBoard: Failed to open %s in rb+", m_sramPath);
        return false;
    }

    // Ensure we are at start
    fseek(m_sramFile, 0, SEEK_SET);
    log_debug("BaseBoard: SRAM file opened successfully");

    return true;
}

void BaseBoard::Close() {
    if (m_sramFile) {
        fclose(m_sramFile);
        m_sramFile = nullptr;
    }
    log_debug("BaseBoard::Close()");
}

int BaseBoard::Read(void* buf, size_t count) {
    // baseboardRead: memcpy from sharedMemory
    if (m_sharedMemoryIndex + count <= sizeof(m_sharedMemory)) {
        memcpy(buf, &m_sharedMemory[m_sharedMemoryIndex], count);
        return (int)count;
    }
    return 0;
}

int BaseBoard::Write(const void* buf, size_t count) {
    // baseboardWrite: memcpy to sharedMemory
    if (m_sharedMemoryIndex + count <= sizeof(m_sharedMemory)) {
        memcpy(&m_sharedMemory[m_sharedMemoryIndex], buf, count);
        return (int)count;
    }
    return 0;
}

int BaseBoard::Ioctl(unsigned long request, void* data) {
    switch (request) {

    case BASEBOARD_GET_VERSION:
        if (data) {
            uint8_t versionData[4] = { 0x00, 0x19, 0x20, 0x07 };
            memcpy(data, versionData, 4);
            log_debug("BaseBoard: GetVersion");
        }
        return 0;

    case BASEBOARD_INIT:
        log_debug("BaseBoard: Init");
        return 0;

    case BASEBOARD_READY:
        log_trace("BaseBoard: Ready");
        return 0;

    case BASEBOARD_SEEK_SHM:
        // The data argument IS the offset (cast to int/size_t)
        m_sharedMemoryIndex = (unsigned int)(uintptr_t)data;
        log_trace("BaseBoard: Seek SHM to %u", m_sharedMemoryIndex);
        return 0;

    case BASEBOARD_READ_SRAM:
        if (data && m_sramFile) {
            struct ReadData { uint32_t ptr; uint32_t offset; uint32_t size; }*rd =
                (ReadData*)data;

            fseek(m_sramFile, rd->offset, SEEK_SET);
            void* dest = (void*)(uintptr_t)rd->ptr;
            if (dest) {
                fread(dest, 1, rd->size, m_sramFile);
                log_trace("BaseBoard: Read SRAM Off=%u Size=%u", rd->offset, rd->size);
            }
        }
        return 0;

    case BASEBOARD_WRITE_SRAM:
        if (data && m_sramFile) {
            struct WriteData { uint32_t offset; uint32_t ptr; uint32_t size; }*wd =
                (WriteData*)data;

            fseek(m_sramFile, wd->offset, SEEK_SET);
            void* src = (void*)(uintptr_t)wd->ptr;
            if (src) {
                fwrite(src, 1, wd->size, m_sramFile);
                fflush(m_sramFile);
                log_trace("BaseBoard: Write SRAM Off=%u Size=%u", wd->offset, wd->size);
            }
        }
        return 0;

    case BASEBOARD_REQUEST:
        if (data) {
            uint32_t* packet = (uint32_t*)data;
            uint32_t cmd = packet[0];

            switch (cmd) {
            case BASEBOARD_GET_SERIAL:
                m_serialCommand.destAddress = packet[1];
                m_serialCommand.destSize = packet[2];
                log_debug("BaseBoard: Request GetSerial");
                break;

            case BASEBOARD_WRITE_FLASH:
                log_warn("BaseBoard: Attempt to write flash (blocked)");
                break;

            case BASEBOARD_PROCESS_JVS:
                m_jvsCommand.srcAddress = packet[1];
                m_jvsCommand.srcSize = packet[2];
                m_jvsCommand.destAddress = packet[3];
                m_jvsCommand.destSize = packet[4];

                if (m_jvsCommand.srcAddress + m_jvsCommand.srcSize <= sizeof(m_sharedMemory)) {
                    memcpy(m_inputBuffer, &m_sharedMemory[m_jvsCommand.srcAddress], m_jvsCommand.srcSize);
                }
                log_trace("BaseBoard: Request ProcessJVS");
                break;

            case BASEBOARD_GET_SENSE_LINE:
                log_trace("BaseBoard: Request GetSenseLine");
                break;

            default:
                log_warn("BaseBoard: Unknown Request 0x%X", cmd);
                break;
            }

            packet[0] |= 0xF0000000;
        }
        return 0;

    case BASEBOARD_RECEIVE:
        if (data) {
            uint32_t* packet = (uint32_t*)data;
            uint32_t cmd = packet[0] & 0xFFF;

            switch (cmd) {
            case BASEBOARD_GET_SERIAL:
                if (m_serialCommand.destAddress + 96 < sizeof(m_sharedMemory)) {
                    strcpy((char*)&m_sharedMemory[m_serialCommand.destAddress + 96], SERIAL_STRING);
                }
                packet[1] = 1;
                log_debug("BaseBoard: Receive GetSerial -> %s", SERIAL_STRING);
                break;

            case BASEBOARD_GET_SENSE_LINE:
                packet[2] = 1;
                packet[1] = 1;
                log_trace("BaseBoard: Receive GetSenseLine -> Connected");
                break;

            case BASEBOARD_PROCESS_JVS:
                packet[2] = m_jvsCommand.destAddress;
                packet[3] = 0;
                packet[1] = 1;
                break;

            default:
                log_trace("BaseBoard: Unknown Receive 0x%X", cmd);
                break;
            }

            packet[0] |= 0xF0000000;
        }
        return 0;

    case 0xBC0B:
        return 0;

    default:
        log_warn("BaseBoard: Unknown Ioctl 0x%lX", request);
        break;
    }

    return 0;
}
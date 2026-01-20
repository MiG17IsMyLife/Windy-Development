#define _CRT_SECURE_NO_WARNINGS

#include "BaseBoard.h"
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
    printf("[BaseBoard] Open\n");

    // lindbergh-loader logic: Ensure file exists, then open rb+
    m_sramFile = fopen(m_sramPath, "a"); // Append mode creates if not exists
    if (m_sramFile) {
        fclose(m_sramFile);
    }
    else {
        printf("[BaseBoard] Error: Cannot open/create %s\n", m_sramPath);
        return false;
    }

    m_sramFile = fopen(m_sramPath, "rb+");
    if (!m_sramFile) {
        printf("[BaseBoard] Error: Failed to open %s in rb+\n", m_sramPath);
        return false;
    }

    // Ensure we are at start
    fseek(m_sramFile, 0, SEEK_SET);

    return true;
}

void BaseBoard::Close() {
    if (m_sramFile) {
        fclose(m_sramFile);
        m_sramFile = nullptr;
    }
    printf("[BaseBoard] Close\n");
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
            // printf("[BaseBoard] GetVersion\n");
        }
        return 0;

    case BASEBOARD_INIT:
        // lindbergh-loader: selectReply = -1 (implied)
        printf("[BaseBoard] Init\n");
        return 0;

    case BASEBOARD_READY:
        // lindbergh-loader: selectReply = 0
        // printf("[BaseBoard] Ready\n");
        return 0;

    case BASEBOARD_SEEK_SHM:
        // The data argument IS the offset (cast to int/size_t)
        m_sharedMemoryIndex = (unsigned int)(uintptr_t)data;
        // printf("[BaseBoard] Seek SHM: %u\n", m_sharedMemoryIndex);
        return 0;

    case BASEBOARD_READ_SRAM:
        if (data && m_sramFile) {
            // struct: { uint32_t *data; uint32_t offset; uint32_t size; }
            struct ReadData { uint32_t ptr; uint32_t offset; uint32_t size; }*rd =
                (ReadData*)data;

            fseek(m_sramFile, rd->offset, SEEK_SET);
            void* dest = (void*)(uintptr_t)rd->ptr;
            if (dest) {
                fread(dest, 1, rd->size, m_sramFile);
                // printf("[BaseBoard] Read SRAM: Off=%u Size=%u\n", rd->offset, rd->size);
            }
        }
        return 0;

    case BASEBOARD_WRITE_SRAM:
        if (data && m_sramFile) {
            // struct: { uint32_t offset; uint32_t *data; uint32_t size; }
            struct WriteData { uint32_t offset; uint32_t ptr; uint32_t size; }*wd =
                (WriteData*)data;

            fseek(m_sramFile, wd->offset, SEEK_SET);
            void* src = (void*)(uintptr_t)wd->ptr;
            if (src) {
                fwrite(src, 1, wd->size, m_sramFile);
                fflush(m_sramFile); // Ensure write to disk immediately
                // printf("[BaseBoard] Write SRAM: Off=%u Size=%u\n", wd->offset, wd->size);
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
                // printf("[BaseBoard] Req: Get Serial\n");
                break;

            case BASEBOARD_WRITE_FLASH:
                printf("[BaseBoard] Warn: Attempt to write flash\n");
                break;

            case BASEBOARD_PROCESS_JVS:
                m_jvsCommand.srcAddress = packet[1];
                m_jvsCommand.srcSize = packet[2];
                m_jvsCommand.destAddress = packet[3];
                m_jvsCommand.destSize = packet[4];

                // Copy to input buffer (Emulate JVS processing start)
                if (m_jvsCommand.srcAddress + m_jvsCommand.srcSize <= sizeof(m_sharedMemory)) {
                    memcpy(m_inputBuffer, &m_sharedMemory[m_jvsCommand.srcAddress], m_jvsCommand.srcSize);
                }
                break;

            case BASEBOARD_GET_SENSE_LINE:
                break;

            default:
                printf("[BaseBoard] Unknown Request: 0x%X\n", cmd);
                break;
            }

            // Acknowledge
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
                    // Inject Serial
                    strcpy((char*)&m_sharedMemory[m_serialCommand.destAddress + 96], SERIAL_STRING);
                }
                packet[1] = 1; // Success
                break;

            case BASEBOARD_GET_SENSE_LINE:
                packet[2] = 1; // 1 = Connected (Dummy)
                packet[1] = 1; // Success
                break;

            case BASEBOARD_PROCESS_JVS:
                // Emulate JVS Response
                // For now, loopback whatever logic or dummy success
                packet[2] = m_jvsCommand.destAddress;
                packet[3] = 0; // packet size 0 for now (dummy)
                packet[1] = 1; // Success/Ready
                break;

            default:
                // printf("[BaseBoard] Unknown Receive: 0x%X\n", cmd);
                break;
            }

            packet[0] |= 0xF0000000;
        }
        return 0;

        // Unknown/Polling
    case 0xBC0B:
        return 0;

    default:
        printf("[BaseBoard] Unknown Ioctl: 0x%lX\n", request);
        break;
    }

    return 0;
}
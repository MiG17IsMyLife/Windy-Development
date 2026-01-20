#define _CRT_SECURE_NO_WARNINGS
#include "BaseBoard.h"
#include <stdio.h>
#include <string.h>

// IOCTL Definitions
#define BASEBOARD_INIT          0x300
#define BASEBOARD_GET_VERSION   0x8004BC02
#define BASEBOARD_SEEK_SHM      0x400
#define BASEBOARD_WRITE_SRAM    0x600
#define BASEBOARD_READ_SRAM     0x601
#define BASEBOARD_REQUEST       0xC020BC06
#define BASEBOARD_RECEIVE       0xC020BC07
#define BASEBOARD_READY         0x201

// Commands
#define CMD_GET_SERIAL          0x120
#define CMD_WRITE_FLASH         0x180
#define CMD_GET_SENSE_LINE      0x210
#define CMD_PROCESS_JVS         0x220

#define SERIAL_STRING "FE11-X018012022X"

BaseBoard::BaseBoard() {
    memset(m_sharedMemory, 0, sizeof(m_sharedMemory));
    memset(&m_jvsCommand, 0, sizeof(m_jvsCommand));
}

BaseBoard::~BaseBoard() {
    Close();
}

bool BaseBoard::Open() {
    printf("[BaseBoard] Open\n");
    // Ensure file exists
    m_sramFile = fopen(m_sramPath, "a");
    if (m_sramFile) fclose(m_sramFile);

    m_sramFile = fopen(m_sramPath, "rb+");
    if (!m_sramFile) return false;

    return true;
}

void BaseBoard::Close() {
    if (m_sramFile) {
        fclose(m_sramFile);
        m_sramFile = nullptr;
    }
}

int BaseBoard::Read(void* buf, size_t count) {
    if (m_sharedMemoryIndex + count <= sizeof(m_sharedMemory)) {
        memcpy(buf, &m_sharedMemory[m_sharedMemoryIndex], count);
        return (int)count;
    }
    return 0;
}

int BaseBoard::Write(const void* buf, size_t count) {
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
        }
        return 0;

    case BASEBOARD_INIT:
    case BASEBOARD_READY:
        return 0;

    case BASEBOARD_SEEK_SHM:
        m_sharedMemoryIndex = (unsigned int)(uintptr_t)data;
        return 0;

    case BASEBOARD_READ_SRAM:
        if (data && m_sramFile) {
            struct ReadData { uint32_t ptr; uint32_t offset; uint32_t size; }*rd = (ReadData*)data;
            fseek(m_sramFile, rd->offset, SEEK_SET);
            void* dest = (void*)(uintptr_t)rd->ptr;
            if (dest) fread(dest, 1, rd->size, m_sramFile);
        }
        return 0;

    case BASEBOARD_WRITE_SRAM:
        if (data && m_sramFile) {
            struct WriteData { uint32_t offset; uint32_t ptr; uint32_t size; }*wd = (WriteData*)data;
            fseek(m_sramFile, wd->offset, SEEK_SET);
            void* src = (void*)(uintptr_t)wd->ptr;
            if (src) {
                fwrite(src, 1, wd->size, m_sramFile);
                fflush(m_sramFile);
            }
        }
        return 0;

    case BASEBOARD_REQUEST:
        if (data) {
            uint32_t* packet = (uint32_t*)data;
            uint32_t cmd = packet[0];
            switch (cmd) {
            case CMD_GET_SERIAL:
                m_serialCommand.destAddress = packet[1];
                m_serialCommand.destSize = packet[2];
                break;
            case CMD_PROCESS_JVS:
                m_jvsCommand.srcAddress = packet[1];
                m_jvsCommand.srcSize = packet[2];
                m_jvsCommand.destAddress = packet[3];
                m_jvsCommand.destSize = packet[4];
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
            case CMD_GET_SERIAL:
                if (m_serialCommand.destAddress + 96 < sizeof(m_sharedMemory)) {
                    strcpy((char*)&m_sharedMemory[m_serialCommand.destAddress + 96], SERIAL_STRING);
                }
                packet[1] = 1;
                break;
            case CMD_GET_SENSE_LINE:
                packet[2] = 1;
                packet[1] = 1;
                break;
            case CMD_PROCESS_JVS:
                // ダミーJVS応答: ステータス1(成功), サイズ0
                packet[2] = m_jvsCommand.destAddress;
                packet[3] = 0;
                packet[1] = 1;
                break;
            }
            packet[0] |= 0xF0000000;
        }
        return 0;

    case 0xBC0B: return 0;
    }
    return 0;
}
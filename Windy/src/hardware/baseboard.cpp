#define _CRT_SECURE_NO_WARNINGS

#include "BaseBoard.h"
#include "JvsBoard.h"
#include "../core/log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SERIAL_STRING "FE11-X018012022X"

typedef struct {
    uint32_t* data;
    uint32_t offset;
    uint32_t size;
} readData_t;

typedef struct {
    uint32_t offset;
    uint32_t* data;
    uint32_t size;
} writeData_t;

BaseBoard::BaseBoard()
    : m_sharedMemoryIndex(0)
    , m_sramFile(nullptr)
    , m_jvsBoard(nullptr)
    , m_jvsPacketSize(-1)
    , m_selectReply(-1)
{
    memset(m_sharedMemory, 0, sizeof(m_sharedMemory));
    memset(&m_jvsCommand, 0, sizeof(m_jvsCommand));
    memset(&m_serialCommand, 0, sizeof(m_serialCommand));
    memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
    memset(m_outputBuffer, 0, sizeof(m_outputBuffer));
    strcpy(m_serialString, SERIAL_STRING);
}

BaseBoard::~BaseBoard() {
    Close();
}

bool BaseBoard::Open() {
    log_info("BaseBoard: Initializing (SRAM: %s)", m_sramPath);

    m_sramFile = fopen(m_sramPath, "a");
    if (m_sramFile == nullptr) {
        log_error("BaseBoard: Cannot open %s", m_sramPath);
        return false;
    }
    fclose(m_sramFile);

    m_sramFile = fopen(m_sramPath, "rb+");
    if (!m_sramFile) return false;
    fseek(m_sramFile, 0, SEEK_SET);

    UpdateSerialString();
    return true;
}

void BaseBoard::UpdateSerialString() {
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    int month = tm_info->tm_mon + 1;
    int day = tm_info->tm_mday;

    strcpy(m_serialString, SERIAL_STRING);

    if ((month == 1 && day == 18) || (month == 5 && day == 20) || (month == 5 && day == 21)) {
        strcpy(m_serialString, "HAPPY BIRTHDAY!!");
    }
}

void BaseBoard::Close() {
    if (m_sramFile) {
        fclose(m_sramFile);
        m_sramFile = nullptr;
    }
}

int BaseBoard::Read(void* buf, size_t count) {
    memcpy(buf, &m_sharedMemory[m_sharedMemoryIndex], count);
    return (int)count;
}

int BaseBoard::Write(const void* buf, size_t count) {
    memcpy(&m_sharedMemory[m_sharedMemoryIndex], buf, count);
    return (int)count;
}

int BaseBoard::Ioctl(unsigned long request, void* data) {
    switch (request) {
    case BASEBOARD_GET_VERSION: {
        uint8_t versionData[4] = { 0x00, 0x19, 0x20, 0x07 };
        memcpy(data, versionData, 4);
        return 0;
    }

    case BASEBOARD_INIT:
        return 0;

    case BASEBOARD_READY:
        m_selectReply = 0;
        return 0;

    case BASEBOARD_SEEK_SHM:

        m_sharedMemoryIndex = (unsigned int)(uintptr_t)data;
        return 0;

    case BASEBOARD_READ_SRAM: {
        readData_t* _data = (readData_t*)data;
        fseek(m_sramFile, _data->offset, SEEK_SET);
        fread(_data->data, 1, _data->size, m_sramFile);
        return 0;
    }

    case BASEBOARD_WRITE_SRAM: {
        writeData_t* _data = (writeData_t*)data;
        fseek(m_sramFile, _data->offset, SEEK_SET);
        fwrite(_data->data, 1, _data->size, m_sramFile);
        fflush(m_sramFile);
        return 0;
    }

    case BASEBOARD_REQUEST: {
        uint32_t* _data = (uint32_t*)data;
        switch (_data[0]) {
        case BASEBOARD_GET_SERIAL:
            m_serialCommand.destAddress = _data[1];
            m_serialCommand.destSize = _data[2];
            break;

        case BASEBOARD_PROCESS_JVS:
            m_jvsCommand.srcAddress = _data[1];
            m_jvsCommand.srcSize = _data[2];
            m_jvsCommand.destAddress = _data[3];
            m_jvsCommand.destSize = _data[4];

            if (m_jvsCommand.srcAddress + m_jvsCommand.srcSize <= sizeof(m_sharedMemory)) {
                memcpy(m_inputBuffer, &m_sharedMemory[m_jvsCommand.srcAddress], m_jvsCommand.srcSize);
                if (m_jvsBoard) {
                    m_jvsBoard->ProcessPacket(m_inputBuffer, m_jvsCommand.srcSize, m_outputBuffer, &m_jvsPacketSize);
                }
            }
            break;

        case BASEBOARD_GET_SENSE_LINE:

            break;

        case BASEBOARD_WRITE_FLASH:
            log_warn("BaseBoard: The game attempted to write to the baseboard flash");
            break;

        default:
            log_warn("BaseBoard: Unknown command %X", _data[0]);
            break;
        }
        _data[0] |= 0xF0000000;
        return 0;
    }

    case BASEBOARD_RECEIVE: {
        uint32_t* _data = (uint32_t*)data;
        switch (_data[0] & 0xFFF) {
        case BASEBOARD_GET_SERIAL:
            if (m_serialCommand.destAddress + 96 + strlen(m_serialString) <= sizeof(m_sharedMemory)) {
                memcpy(&m_sharedMemory[m_serialCommand.destAddress + 96], m_serialString, strlen(m_serialString));
            }
            _data[1] = 1;
            break;

        case BASEBOARD_GET_SENSE_LINE:
            _data[2] = m_jvsBoard ? m_jvsBoard->GetSenseLine() : 3;
            _data[1] = 1;
            break;

        case BASEBOARD_PROCESS_JVS:
            if (m_jvsCommand.destAddress + m_jvsPacketSize <= sizeof(m_sharedMemory)) {
                memcpy(&m_sharedMemory[m_jvsCommand.destAddress], m_outputBuffer, m_jvsPacketSize);
                _data[2] = m_jvsCommand.destAddress;
                _data[3] = m_jvsPacketSize;
                _data[1] = 1;
            }
            break;

        default:
            log_warn("BaseBoard: Unknown receive command %X", _data[0] & 0xFFF);
            break;
        }
        _data[0] |= 0xF0000000;
        return 0;
    }

    default:
        log_warn("BaseBoard: Unknown Ioctl %lX", request);
        break;
    }
    return -1;
}
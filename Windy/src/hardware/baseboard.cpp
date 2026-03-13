#define _CRT_SECURE_NO_WARNINGS

#include "baseboard.h"
#include "jvsboard.h"
#include "../core/log.h"
#include <cstring>
#include <cstdio>
#include <ctime>

// ============================================================
// IOCTL Command Definitions
// ============================================================

#define BASEBOARD_INIT 0x300
#define BASEBOARD_GET_VERSION 0x8004BC02
#define BASEBOARD_SEEK_SHM 0x400
#define BASEBOARD_READ_SRAM 0x601
#define BASEBOARD_WRITE_SRAM 0x600
#define BASEBOARD_REQUEST 0xC020BC06
#define BASEBOARD_RECEIVE 0xC020BC07
#define BASEBOARD_GET_SERIAL 0x120
#define BASEBOARD_WRITE_FLASH 0x180
#define BASEBOARD_GET_SENSE_LINE 0x210
#define BASEBOARD_PROCESS_JVS 0x220
#define BASEBOARD_READY 0x201

// Default serial string
#define SERIAL_STRING "FE11-X018012022X"

// ============================================================
// Data structures for SRAM read/write
// ============================================================

typedef struct
{
    uint32_t *data;
    uint32_t offset;
    uint32_t size;
} ReadData_t;

typedef struct
{
    uint32_t offset;
    uint32_t *data;
    uint32_t size;
} WriteData_t;

// ============================================================
// Implementation
// ============================================================

BaseBoard::BaseBoard() : m_jvsBoard(nullptr), m_sram(nullptr), m_sharedMemoryIndex(0), m_selectReply(-1), m_jvsPacketSize(-1)
{
    memset(m_sharedMemory, 0, sizeof(m_sharedMemory));
    memset(m_serialString, 0, sizeof(m_serialString));
    memset(&m_jvsCommand, 0, sizeof(m_jvsCommand));
    memset(&m_serialCommand, 0, sizeof(m_serialCommand));

    // Set default serial string
    strcpy(m_serialString, SERIAL_STRING);
}

BaseBoard::~BaseBoard()
{
    Close();
}

bool BaseBoard::Open()
{
    log_info("BaseBoard: Opening...");

    // Initialize serial string with possible Easter egg
    strcpy(m_serialString, SERIAL_STRING);

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);

    int month = tm_info->tm_mon + 1;
    int day = tm_info->tm_mday;

    if ((month == 1 && day == 18) || (month == 5 && day == 20) || (month == 5 && day == 21))
    {
        strcpy(m_serialString, "HAPPY BIRTHDAY!!");
    }

    // Open/create SRAM file
    if (!m_sramPath.empty())
    {
        // Create file if it doesn't exist
        m_sram = fopen(m_sramPath.c_str(), "a");
        if (m_sram == NULL)
        {
            log_error("BaseBoard: Cannot open %s", m_sramPath.c_str());
            return false;
        }
        fclose(m_sram);

        // Reopen for read/write
        m_sram = fopen(m_sramPath.c_str(), "rb+");
        if (m_sram)
        {
            fseek(m_sram, 0, SEEK_SET);
            log_info("BaseBoard: SRAM file opened: %s", m_sramPath.c_str());
        }
    }

    return true;
}

void BaseBoard::Close()
{
    if (m_sram)
    {
        fclose(m_sram);
        m_sram = nullptr;
        log_info("BaseBoard: SRAM file closed");
    }
}

void BaseBoard::SetSramPath(const char *path)
{
    if (path)
    {
        m_sramPath = path;

        // Close existing SRAM file if open
        if (m_sram)
        {
            fclose(m_sram);
            m_sram = nullptr;
        }

        // Create file if it doesn't exist
        FILE *f = fopen(m_sramPath.c_str(), "a");
        if (f == NULL)
        {
            log_error("BaseBoard: Cannot create SRAM file %s", m_sramPath.c_str());
            return;
        }
        fclose(f);

        // Open for read/write
        m_sram = fopen(m_sramPath.c_str(), "rb+");
        if (m_sram)
        {
            fseek(m_sram, 0, SEEK_SET);
            log_info("BaseBoard: SRAM file opened: %s", m_sramPath.c_str());
        }
        else
        {
            log_error("BaseBoard: Failed to open SRAM file for read/write: %s", m_sramPath.c_str());
        }
    }
}

int BaseBoard::Read(char *buf, size_t count)
{
    // Read from shared memory at current index
    if (buf && count > 0)
    {
        memcpy(buf, &m_sharedMemory[m_sharedMemoryIndex], count);
    }
    return (int)count;
}

int BaseBoard::Write(const char *buf, size_t count)
{
    // Write to shared memory at current index
    if (buf && count > 0)
    {
        memcpy(&m_sharedMemory[m_sharedMemoryIndex], buf, count);
    }
    return (int)count;
}

int BaseBoard::Select()
{
    return m_selectReply;
}

int BaseBoard::Ioctl(unsigned int request, void *data)
{
    switch (request)
    {

        case BASEBOARD_GET_VERSION:
        {
            // Return baseboard version
            uint8_t versionData[4] = {0x00, 0x19, 0x20, 0x07};
            memcpy(data, versionData, 4);
            log_debug("BaseBoard: BASEBOARD_GET_VERSION");
            return 0;
        }

        case BASEBOARD_INIT:
        {
            log_debug("BaseBoard: BASEBOARD_INIT");
            // selectReply = -1;
            return 0;
        }

        case BASEBOARD_READY:
        {
            log_debug("BaseBoard: BASEBOARD_READY");
            m_selectReply = 0;
            return 0;
        }

        case BASEBOARD_SEEK_SHM:
        {
            // Set shared memory index
            m_sharedMemoryIndex = (uint32_t)(uintptr_t)data;
            log_trace("BaseBoard: BASEBOARD_SEEK_SHM index=%u", m_sharedMemoryIndex);
            return 0;
        }

        case BASEBOARD_READ_SRAM:
        {
            ReadData_t *_data = (ReadData_t *)data;
            if (m_sram && _data)
            {
                fseek(m_sram, _data->offset, SEEK_SET);
                fread(_data->data, 1, _data->size, m_sram);
                log_debug("BaseBoard: BASEBOARD_READ_SRAM offset=%u size=%u", _data->offset, _data->size);
            }
            return 0;
        }

        case BASEBOARD_WRITE_SRAM:
        {
            WriteData_t *_data = (WriteData_t *)data;
            if (m_sram && _data)
            {
                fseek(m_sram, _data->offset, SEEK_SET);
                fwrite(_data->data, 1, _data->size, m_sram);
                fflush(m_sram); // Ensure data is written
                log_debug("BaseBoard: BASEBOARD_WRITE_SRAM offset=%u size=%u", _data->offset, _data->size);
            }
            return 0;
        }

        case BASEBOARD_REQUEST:
        {
            ProcessRequest((uint32_t *)data);
            return 0;
        }

        case BASEBOARD_RECEIVE:
        {
            ProcessReceive((uint32_t *)data);
            return 0;
        }

        default:
            log_warn("BaseBoard: Unknown IOCTL 0x%04X", request);
            return 0; // Return success to avoid blocking
    }

    return 0;
}

void BaseBoard::ProcessRequest(uint32_t *data)
{
    if (!data)
        return;

    uint32_t command = data[0];

    switch (command)
    {

        case BASEBOARD_GET_SERIAL: // bcCmdSysInfoGetReq (0x120)
        {
            m_serialCommand.destAddress = data[1];
            m_serialCommand.destSize = data[2];
            log_debug("BaseBoard: REQUEST GET_SERIAL destAddr=%u destSize=%u", m_serialCommand.destAddress, m_serialCommand.destSize);
        }
        break;

        case BASEBOARD_WRITE_FLASH: // bcCmdSysFlashWrite (0x180)
        {
            log_warn("BaseBoard: The game attempted to write to the baseboard flash");
        }
        break;

        case BASEBOARD_PROCESS_JVS: // 0x220
        {
            m_jvsCommand.srcAddress = data[1];
            m_jvsCommand.srcSize = data[2];
            m_jvsCommand.destAddress = data[3];
            m_jvsCommand.destSize = data[4];

            log_trace("BaseBoard: REQUEST PROCESS_JVS src=%u/%u dest=%u/%u", m_jvsCommand.srcAddress, m_jvsCommand.srcSize,
                      m_jvsCommand.destAddress, m_jvsCommand.destSize);

            // Process JVS packet if JVS board is available
            if (m_jvsBoard && m_jvsCommand.srcSize > 0)
            {
                // Copy input from shared memory
                uint8_t inputBuffer[256];
                uint8_t outputBuffer[256];

                size_t copySize = (m_jvsCommand.srcSize < sizeof(inputBuffer)) ? m_jvsCommand.srcSize : sizeof(inputBuffer);
                memcpy(inputBuffer, &m_sharedMemory[m_jvsCommand.srcAddress], copySize);

                // Process JVS packet
                int outputSize = 0;
                m_jvsBoard->ProcessPacket(inputBuffer, (unsigned int)copySize, outputBuffer, &outputSize);
                m_jvsPacketSize = outputSize;

                // Store output in shared memory for later RECEIVE
                if (outputSize > 0)
                {
                    memcpy(&m_sharedMemory[m_jvsCommand.destAddress], outputBuffer, outputSize);
                }
            }
        }
        break;

        case BASEBOARD_GET_SENSE_LINE: // 0x210
        {
            log_trace("BaseBoard: REQUEST GET_SENSE_LINE");
            // Just acknowledge, actual response in RECEIVE
        }
        break;

        default:
            log_warn("BaseBoard: Unknown REQUEST command 0x%X", command);
    }

    // Acknowledge the command
    data[0] |= 0xF0000000;
}

void BaseBoard::ProcessReceive(uint32_t *data)
{
    if (!data)
        return;

    uint32_t command = data[0] & 0xFFF;

    switch (command)
    {

        case BASEBOARD_GET_SERIAL: // 0x120
        {
            // Copy serial string to shared memory at offset 96 from destination
            memcpy(&m_sharedMemory[m_serialCommand.destAddress + 96], m_serialString, strlen(m_serialString));
            data[1] = 1; // Set status to success
            log_debug("BaseBoard: RECEIVE GET_SERIAL -> '%s'", m_serialString);
        }
        break;

        case BASEBOARD_GET_SENSE_LINE: // 0x210
        {
            // Return sense line value
            int senseLine = 3; // Default value
            if (m_jvsBoard)
            {
                senseLine = m_jvsBoard->GetSenseLine();
            }
            data[2] = senseLine;
            data[1] = 1; // Set status to success
            log_trace("BaseBoard: RECEIVE GET_SENSE_LINE -> %d", senseLine);
        }
        break;

        case BASEBOARD_PROCESS_JVS: // 0x220
        {
            // Return JVS response location and size
            data[2] = m_jvsCommand.destAddress;
            data[3] = m_jvsPacketSize;
            data[1] = (m_jvsPacketSize > 0) ? 1 : 0; // Status: 1=success, 0=no data
            log_trace("BaseBoard: RECEIVE PROCESS_JVS -> addr=%u size=%d", m_jvsCommand.destAddress, m_jvsPacketSize);
        }
        break;

        default:
            log_warn("BaseBoard: Unknown RECEIVE command 0x%X", command);
    }

    // Acknowledge the command
    data[0] |= 0xF0000000;
}
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <cstring>

#pragma warning(push)
#pragma warning(disable : 4996)

// Forward declaration
class JvsBoard;

// --- IOCTL Definitions ---
#define BASEBOARD_INIT          0x300
#define BASEBOARD_GET_VERSION   0x8004BC02
#define BASEBOARD_SEEK_SHM      0x400
#define BASEBOARD_READ_SRAM     0x601
#define BASEBOARD_WRITE_SRAM    0x600
#define BASEBOARD_REQUEST       0xC020BC06
#define BASEBOARD_RECEIVE       0xC020BC07
#define BASEBOARD_READY         0x201

// Request IDs (Internal to BASEBOARD_REQUEST)
#define BASEBOARD_GET_SERIAL        0x120
#define BASEBOARD_WRITE_FLASH       0x180
#define BASEBOARD_GET_SENSE_LINE    0x210
#define BASEBOARD_PROCESS_JVS       0x220

class BaseBoard {
public:
    BaseBoard();
    ~BaseBoard();

    bool Open();
    void Close();

    int Read(void* buf, size_t count);
    int Write(const void* buf, size_t count);
    int Ioctl(unsigned long request, void* data);

    void SetJvsBoard(JvsBoard* jvs) { m_jvsBoard = jvs; }

    void SetSramPath(const char* path) {
        if (path) strncpy(m_sramPath, path, sizeof(m_sramPath) - 1);
    }

private:
    struct BaseboardCommand {
        uint32_t srcAddress;
        uint32_t srcSize;
        uint32_t destAddress;
        uint32_t destSize;
    };

    void UpdateSerialString();

    uint8_t m_sharedMemory[1024 * 32];
    unsigned int m_sharedMemoryIndex;

    FILE* m_sramFile;
    char m_sramPath[1024] = "sram.bin";
    char m_serialString[1024];

    BaseboardCommand m_jvsCommand;
    BaseboardCommand m_serialCommand;

    JvsBoard* m_jvsBoard = nullptr;
    uint8_t m_inputBuffer[1024 * 4];
    uint8_t m_outputBuffer[1024 * 4];
    int m_jvsPacketSize;
    int m_selectReply;
};

#pragma warning(pop)
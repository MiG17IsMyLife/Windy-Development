#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>

class JvsBoard;  // Forward declaration

/**
 * @brief Base Board emulation (/dev/lbb)
 * Handles:
 * - Shared memory access
 * - SRAM read/write
 * - JVS communication relay
 * - Serial number
 * - Various IOCTL commands
 */
class BaseBoard {
public:
    BaseBoard();
    ~BaseBoard();

    bool Open();
    void Close();

    // File operations
    int Read(char* buf, size_t count);
    int Write(const char* buf, size_t count);
    int Ioctl(unsigned int request, void* data);
    int Select();  // Returns selectReply value

    // Configuration
    void SetSramPath(const char* path);
    void SetJvsBoard(JvsBoard* jvs) { m_jvsBoard = jvs; }

private:
    // Process BASEBOARD_REQUEST sub-commands
    void ProcessRequest(uint32_t* data);
    // Process BASEBOARD_RECEIVE sub-commands  
    void ProcessReceive(uint32_t* data);

private:
    JvsBoard* m_jvsBoard;

    // SRAM file
    std::string m_sramPath;
    FILE* m_sram;

    // Shared memory (32KB)
    uint8_t m_sharedMemory[1024 * 32];
    uint32_t m_sharedMemoryIndex;

    // Select reply value
    int m_selectReply;

    // Serial string
    char m_serialString[1024];

    // JVS command state
    struct {
        uint32_t srcAddress;
        uint32_t srcSize;
        uint32_t destAddress;
        uint32_t destSize;
    } m_jvsCommand;

    // Serial command state
    struct {
        uint32_t destAddress;
        uint32_t destSize;
    } m_serialCommand;

    // JVS packet size (for emulated JVS)
    int m_jvsPacketSize;
};
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>

class JvsBoard;  // Forward declaration

/**
 * @brief Base Board emulation
 *
 * Handles /dev/lbb device emulation including:
 * - SRAM access
 * - JVS communication relay
 * - IOCTL commands
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

    // Configuration
    void SetSramPath(const char* path);
    void SetJvsBoard(JvsBoard* jvs) { m_jvsBoard = jvs; }

private:
    JvsBoard* m_jvsBoard;
    std::string m_sramPath;
    uint8_t m_sram[32768];  // 32KB SRAM
    size_t m_sramSize;

    // JVS response buffer
    uint8_t m_jvsResponse[256];
    int m_jvsResponseLen;
};
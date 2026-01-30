#pragma once

#include <stdint.h>

/**
 * @brief Serial Board emulation
 *
 * Handles serial communication for various peripherals:
 * - Driveboard (force feedback for driving games)
 * - Motionboard (motion control)
 * - Card readers
 */
class SerialBoard {
public:
    SerialBoard();
    ~SerialBoard();

    bool Open();
    void Close();

    // Use unsigned int for MSVC 32-bit compatibility
    int Read(char* buf, unsigned int count);
    int Write(const char* buf, unsigned int count);
    int Ioctl(unsigned int request, void* data);
};
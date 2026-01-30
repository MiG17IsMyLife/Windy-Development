#pragma once

#include <stdint.h>

/**
 * @brief Security Board / DIP Switch emulation
 *
 * Handles I/O port access for:
 * - DIP switch configuration
 * - Resolution settings
 * - Security chip emulation
 *
 * Port addresses (from Lindbergh-loader):
 * - 0x38: Primary security port
 * - 0x3F: Secondary security port
 */
class SecurityBoard {
public:
    SecurityBoard();
    ~SecurityBoard();

    /**
     * @brief Read from I/O port
     * @param port Port address
     * @param data Output data
     * @return 0 on success
     */
    int PortRead(uint16_t port, uint32_t* data);

    /**
     * @brief Write to I/O port
     * @param port Port address
     * @param data Data to write
     * @return 0 on success
     */
    int PortWrite(uint16_t port, uint32_t data);

    /**
     * @brief Set DIP switch resolution
     * @param width Display width
     * @param height Display height
     */
    void SetDipResolution(int width, int height);

    /**
     * @brief Get current DIP switch value
     */
    uint8_t GetDipSwitch() const { return m_dipSwitch; }

private:
    uint8_t m_dipSwitch;
    int m_width;
    int m_height;
};
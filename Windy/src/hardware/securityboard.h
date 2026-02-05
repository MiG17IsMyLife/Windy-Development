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

#define SECURITY_BOARD_FRONT_PANEL          0x38
#define SECURITY_BOARD_FRONT_PANEL_NON_ROOT 0x39

    /**
     * @brief Set DIP switch resolution
     * @param width Display width
     * @param height Display height
     */
    void SetDipResolution(int width, int height);

    /**
     * @brief Get current DIP switch value
     */
    // uint8_t GetDipSwitch() const { return m_dipSwitch; } // replaced by public members

// Public members to match requested global access pattern
public:
    int dipSwitch[9]; // 1-based indexing assumed by user code
    int serviceSwitch;
    int testSwitch;

    // Helper for front panel I/O
    int SecurityBoardIn(uint16_t port, uint32_t *data);

private:
    int m_width;
    int m_height;
};

extern SecurityBoard& securityBoard; // Global reference for user code compatibility
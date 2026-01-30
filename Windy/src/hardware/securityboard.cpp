#include "securityboard.h"
#include "../core/log.h"

// I/O Port addresses
#define PORT_SECURITY_PRIMARY   0x38
#define PORT_SECURITY_SECONDARY 0x3F

SecurityBoard::SecurityBoard()
    : m_dipSwitch(0)
    , m_width(640)
    , m_height(480)
{
}

SecurityBoard::~SecurityBoard() {
}

int SecurityBoard::PortRead(uint16_t port, uint32_t* data) {
    if (!data) return -1;

    switch (port) {
    case PORT_SECURITY_PRIMARY:
    case PORT_SECURITY_SECONDARY:
        *data = m_dipSwitch;
        return 0;

    default:
        // Unknown port - return 0xFF (typical for unconnected I/O)
        *data = 0xFF;
        return 0;
    }
}

int SecurityBoard::PortWrite(uint16_t port, uint32_t data) {
    switch (port) {
    case PORT_SECURITY_PRIMARY:
    case PORT_SECURITY_SECONDARY:
        // DIP switch writes are typically ignored (read-only hardware)
        log_debug("SecurityBoard: Write to port 0x%02X: 0x%02X (ignored)", port, data);
        return 0;

    default:
        return 0;
    }
}

void SecurityBoard::SetDipResolution(int width, int height) {
    m_width = width;
    m_height = height;

    // Calculate DIP switch setting based on resolution
    // (Matches Lindbergh-loader's getConfig()->lindpiSwitch logic)

    // Resolution DIP encoding (bits 0-3):
    // 0x00 = 640x480
    // 0x01 = 800x480 (OutRun2 wide)
    // 0x02 = 1024x600
    // 0x03 = 1024x768
    // 0x04 = 1280x720
    // 0x05 = 1280x768
    // 0x06 = 1360x768
    // 0x07 = 1920x1080

    if (width <= 640 && height <= 480) {
        m_dipSwitch = 0x00;
    }
    else if (width <= 800 && height <= 480) {
        m_dipSwitch = 0x01;
    }
    else if (width <= 1024 && height <= 600) {
        m_dipSwitch = 0x02;
    }
    else if (width <= 1024 && height <= 768) {
        m_dipSwitch = 0x03;
    }
    else if (width <= 1280 && height <= 720) {
        m_dipSwitch = 0x04;
    }
    else if (width <= 1280 && height <= 768) {
        m_dipSwitch = 0x05;
    }
    else if (width <= 1360 && height <= 768) {
        m_dipSwitch = 0x06;
    }
    else {
        m_dipSwitch = 0x07;  // 1920x1080 or higher
    }

    log_info("SecurityBoard: DIP switch set to 0x%02X for %dx%d", m_dipSwitch, width, height);
}
#include "securityboard.h"
#include "../core/log.h"
#include "lindberghdevice.h" // For singleton access

// Global reference initialization
SecurityBoard& securityBoard = *LindberghDevice::Instance().GetSecurityBoard();

// I/O Port addresses
#define PORT_SECURITY_PRIMARY   0x38
#define PORT_SECURITY_SECONDARY 0x3F

SecurityBoard::SecurityBoard()
    : serviceSwitch(0)
    , testSwitch(0)
    , m_width(640)
    , m_height(480)
{
    for (int i = 0; i < 9; i++) dipSwitch[i] = 0;
}

SecurityBoard::~SecurityBoard() {
}

int SecurityBoard::PortRead(uint16_t port, uint32_t* data) {
    if (!data) return -1;
    
    // Delegate to the member function
    if (port == SECURITY_BOARD_FRONT_PANEL || port == SECURITY_BOARD_FRONT_PANEL_NON_ROOT) {
        return SecurityBoardIn(port, data);
    }

    switch (port) {
    case PORT_SECURITY_PRIMARY:
    case PORT_SECURITY_SECONDARY:
        // Fallback or legacy handling
        *data = 0xFF; 
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

    // Reset all
    for (int i = 0; i < 9; i++) dipSwitch[i] = 0;

    // Resolution DIP encoding (bits 0-3 correspond to DIP 1-4?)
    // Re-interpreting the legacy m_dipSwitch mapping to the new array
    // Based on user code: checks dipSwitch[1]..dipSwitch[6] etc.
    
    uint8_t dipValue = 0x00;

    if (width <= 640 && height <= 480) {
        dipValue = 0x00;
    }
    else if (width <= 800 && height <= 480) {
        dipValue = 0x01;
    }
    else if (width <= 1024 && height <= 600) {
        dipValue = 0x02;
    }
    else if (width <= 1024 && height <= 768) {
        dipValue = 0x03;
    }
    else if (width <= 1280 && height <= 720) {
        dipValue = 0x04;
    }
    else if (width <= 1280 && height <= 768) {
        dipValue = 0x05;
    }
    else if (width <= 1360 && height <= 768) {
        dipValue = 0x06;
    }
    else {
        dipValue = 0x07;  // 1920x1080 or higher
    }

    // Map bits to dipSwitch array (1-based because user code uses index 1..8)
    // Assuming standard binary mapping: bit 0 -> DIP 1, bit 1 -> DIP 2, etc.
    if (dipValue & 0x01) dipSwitch[1] = 1;
    if (dipValue & 0x02) dipSwitch[2] = 1;
    if (dipValue & 0x04) dipSwitch[3] = 1;
    if (dipValue & 0x08) dipSwitch[4] = 1;
    
    // Bits 4-5 ..? 
    // The user code seems to check:
    // dipSwitch[6] (bit12)
    // dipSwitch[5] (bit11) ...
    
    // For now, I'll stick to the resolution mapping we had.
    // Note: User code for SetDipResolution previously set a single byte.
    // The user provided `securityBoardIn` logic implies specific DIP meanings.
    
    log_info("SecurityBoard: DIP switch set to 0x%02X for %dx%d", dipValue, width, height);
}

int SecurityBoard::SecurityBoardIn(uint16_t port, uint32_t *data)
{
    switch (port)
    {
        case SECURITY_BOARD_FRONT_PANEL_NON_ROOT:
        case SECURITY_BOARD_FRONT_PANEL:
        {
            uint32_t result = 0xFFFFFFFF;
            if (dipSwitch[6]) // bit12
                result &= ~0x800;           // DIP 6
            if (dipSwitch[5]) // bit11
                result &= ~0x400;           // DIP 5
            if (dipSwitch[4]) // bit10
                result &= ~0x200;           //  DIP 4
            if (dipSwitch[3]) // bit9
                result &= ~0x100;           //  DIP 3
            if (dipSwitch[2]) // bit8
                result &= ~0x80;            // DIP 2
            if (dipSwitch[1]) // bit7
                result &= ~0x40;            // DIP 1
            if (dipSwitch[8]) // bit6
                result &= ~0x20;
            if (dipSwitch[7]) // bit5
                result &= ~0x10;
            if (serviceSwitch) // bit4
                result &= ~0x08;
            if (testSwitch) // bit3
                result &= ~0x04;
            *data = result;
        }
        break;

        default:
            break;
    }

    return 0;
}
#include "SecurityBoard.h"
#include "../core/log.h"
#include <stdio.h>
#include <cstring>

// Port Definitions
#define SECURITY_BOARD_FRONT_PANEL 0x38
#define SECURITY_BOARD_FRONT_PANEL_NON_ROOT 0x1038

// Dip Switch Constants
#define DIP_SWITCH_ROTATION 3

SecurityBoard::SecurityBoard()
    : m_serviceSwitch(0)
    , m_testSwitch(0)
{
    // Initialize DIP switches (Index 1-8)
    for (int i = 0; i < 9; i++) {
        m_dipSwitch[i] = 0;
    }
}

SecurityBoard::~SecurityBoard() {
}

void SecurityBoard::SetResolutionDips(int dip4, int dip5, int dip6) {
    m_dipSwitch[4] = dip4;
    m_dipSwitch[5] = dip5;
    m_dipSwitch[6] = dip6;
}

void SecurityBoard::SetDipResolution(int width, int height) {
    if (width == 640 && height == 480)
        SetResolutionDips(0, 0, 0);
    else if (width == 800 && height == 600)
        SetResolutionDips(0, 0, 1);
    else if (width == 1024 && height == 768)
        SetResolutionDips(0, 1, 0);
    else if (width == 1280 && height == 1024)
        SetResolutionDips(0, 1, 1);
    else if (width == 800 && height == 480)
        SetResolutionDips(1, 0, 0);
    else if (width == 1024 && height == 600)
        SetResolutionDips(1, 0, 1);
    else if (width == 1280 && height == 768)
        SetResolutionDips(1, 1, 0);
    else if (width == 1360 && height == 768)
        SetResolutionDips(1, 1, 1);
    else {
        log_warn("SecurityBoard: Resolution %dx%d not original, setting dips to 640x480.", width, height);
        SetResolutionDips(0, 0, 0);
    }
}

void SecurityBoard::SetRotation(int rotation) {
    m_dipSwitch[DIP_SWITCH_ROTATION] = rotation;
}

void SecurityBoard::SetDipSwitch(int switchNumber, int value) {
    if (switchNumber < 1 || switchNumber > 8) {
        log_error("SecurityBoard: Dip Switch index %d out of range (1-8)", switchNumber);
        return;
    }
    m_dipSwitch[switchNumber] = value;
}

void SecurityBoard::SetSwitch(JVSInput switchNumber, int value) {
    switch (switchNumber) {
    case BUTTON_TEST:
        m_testSwitch = value;
        break;
    case BUTTON_SERVICE:
        m_serviceSwitch = value;
        break;
    default:
        log_warn("SecurityBoard: Attempted to set incorrect switch on security board");
        break;
    }
}

int SecurityBoard::Out(uint16_t port, uint32_t* data) {
    return 0;
}

int SecurityBoard::In(uint16_t port, uint32_t* data) {
    switch (port) {
    case SECURITY_BOARD_FRONT_PANEL_NON_ROOT:
    case SECURITY_BOARD_FRONT_PANEL:
    {
        uint32_t result = 0xFFFFFFFF;

        if (m_dipSwitch[6]) result &= ~0x800;
        if (m_dipSwitch[5]) result &= ~0x400;
        if (m_dipSwitch[4]) result &= ~0x200;
        if (m_dipSwitch[3]) result &= ~0x100;
        if (m_dipSwitch[2]) result &= ~0x80;
        if (m_dipSwitch[1]) result &= ~0x40;
        if (m_dipSwitch[8]) result &= ~0x20;
        if (m_dipSwitch[7]) result &= ~0x10;

        if (m_serviceSwitch) result &= ~0x08;
        if (m_testSwitch)    result &= ~0x04;

        *data = result;
    }
    break;

    default:
        break;
    }
    return 0;
}
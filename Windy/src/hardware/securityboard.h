#pragma once

#include <stdint.h>
#include "jvsboard.h" // For JVSInput enum

class SecurityBoard {
public:
    SecurityBoard();
    ~SecurityBoard();

    // Port I/O wrappers
    int In(uint16_t port, uint32_t* data);
    int Out(uint16_t port, uint32_t* data);

    // Input & Configuration
    void SetSwitch(JVSInput switchNumber, int value);
    void SetDipSwitch(int switchNumber, int value);
    void SetDipResolution(int width, int height);
    void SetRotation(int rotation);

private:
    void SetResolutionDips(int dip4, int dip5, int dip6);

    int m_serviceSwitch;
    int m_testSwitch;

    // DIP Switches (Index 1-8 used to match loader logic)
    int m_dipSwitch[9];
};

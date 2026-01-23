#pragma once
#include "LindberghDevice.h"
#include <vector>
#include <string>
#include <stdint.h>

// JVS Protocol Constants
#define JVS_SYNC        0xE0
#define JVS_ESCAPE      0xD0
#define JVS_BROADCAST   0xFF
#define JVS_BUS_MASTER  0x00

// Status Codes
#define JVS_STATUS_SUCCESS          0x01
#define JVS_STATUS_UNKNOWN_CMD      0x02
#define JVS_STATUS_SUM_ERROR        0x03
#define JVS_STATUS_BUSY             0x04

// Report Codes
#define JVS_REPORT_SUCCESS          0x01
#define JVS_REPORT_PARAM_ERROR      0x02
#define JVS_REPORT_DATA_ERROR       0x03
#define JVS_REPORT_BUSY             0x04

// Commands
#define CMD_RESET                   0xF0
#define CMD_ASSIGN_ADDR             0xF1
#define CMD_SET_COMMS_MODE          0xF2
#define CMD_REQUEST_ID              0x10
#define CMD_COMMAND_VERSION         0x11
#define CMD_JVS_VERSION             0x12
#define CMD_COMMS_VERSION           0x13
#define CMD_CAPABILITIES            0x14
#define CMD_CONVEY_ID               0x15
#define CMD_READ_SWITCHES           0x20
#define CMD_READ_COINS              0x21
#define CMD_READ_ANALOGS            0x22
#define CMD_READ_ROTARY             0x23
#define CMD_READ_KEYPAD             0x24
#define CMD_READ_GPI                0x26
#define CMD_WRITE_GPO               0x32
#define CMD_WRITE_ANALOG            0x33
#define CMD_WRITE_DISPLAY           0x34
#define CMD_WRITE_COINS             0x35
#define CMD_SUBTRACT_PAYOUT         0x36
#define CMD_WRITE_GPO_BYTE          0x37
#define CMD_WRITE_GPO_BIT           0x38

// Input Mappings (Bitmasks)
#define SW_TEST     0x80
#define SW_SERVICE  0x40
#define SW_START    0x80
#define SW_UP       0x20
#define SW_DOWN     0x10
#define SW_LEFT     0x08
#define SW_RIGHT    0x04
#define SW_BTN1     0x02
#define SW_BTN2     0x01
#define SW_BTN3     0x80 // Byte 2
#define SW_BTN4     0x40 // Byte 2

struct JvsCapabilities {
    int players;
    int switches;
    int coins;
    int analogueInChannels;
    int analogueInBits;
    int rotaryChannels;
    int keypad;
    int gunChannels;
    int generalPurposeInputs;
    int card;
    int hopper;
    int generalPurposeOutputs;
    int analogueOutChannels;
    int displayOutRows;
    int displayOutColumns;
    int backup;
    std::string name;
};

class JvsBoard : public LindberghDevice {
private:
    // buffers
    std::vector<uint8_t> m_inputBuffer;  // Raw bytes from host
    std::vector<uint8_t> m_outputBuffer; // Raw bytes to host (response)

    // State
    uint8_t m_address;
    bool m_senseLine;
    JvsCapabilities m_caps;

    // Coin management
    uint16_t m_coinCount[2];
    bool m_coinPressed;

    // Helper methods
    void ProcessPacket(const std::vector<uint8_t>& packet);
    void WritePacket(const std::vector<uint8_t>& data);
    void WriteFeature(std::vector<uint8_t>& out, uint8_t cap, uint8_t arg1, uint8_t arg2, uint8_t arg3);

public:
    JvsBoard();
    virtual ~JvsBoard();

    bool Open() override;
    void Close() override;
    int Read(void* buf, size_t count) override;
    int Write(const void* buf, size_t count) override;
    int Ioctl(unsigned long request, void* data) override;

    // Additional helper to get raw state if needed by BaseBoard
    int GetSenseLine() { return m_senseLine ? 1 : 3; } // 1=Ready, 3=Disconnected
};
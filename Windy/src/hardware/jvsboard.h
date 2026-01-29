#pragma once

#include <vector>
#include <string>
#include <stdint.h>
#include <mutex>

// JVS Constants
#define JVS_MAX_PACKET_SIZE 255
#define JVS_MAX_STATE_SIZE 100
#define MAX_JVS_NAME_SIZE 2048

// JVS Commands
#define CMD_RESET 0xF0
#define CMD_ASSIGN_ADDR 0xF1
#define CMD_SET_COMMS_MODE 0xF2
#define CMD_REQUEST_ID 0x10
#define CMD_COMMAND_VERSION 0x11
#define CMD_JVS_VERSION 0x12
#define CMD_COMMS_VERSION 0x13
#define CMD_CAPABILITIES 0x14
#define CMD_CONVEY_ID 0x15
#define CMD_READ_SWITCHES 0x20
#define CMD_READ_COINS 0x21
#define CMD_READ_ANALOGS 0x22
#define CMD_READ_ROTARY 0x23
#define CMD_READ_KEYPAD 0x24
#define CMD_READ_LIGHTGUN 0x25
#define CMD_READ_GPI 0x26
#define CMD_RETRANSMIT 0x2F
#define CMD_DECREASE_COINS 0x30
#define CMD_WRITE_GPO 0x32
#define CMD_WRITE_ANALOG 0x33
#define CMD_WRITE_DISPLAY 0x34
#define CMD_WRITE_COINS 0x35
#define CMD_REMAINING_PAYOUT 0x2E
#define CMD_SET_PAYOUT 0x31
#define CMD_SUBTRACT_PAYOUT 0x36
#define CMD_WRITE_GPO_BYTE 0x37
#define CMD_WRITE_GPO_BIT 0x38

// Capabilities Definitions
#define CAP_END 0x00
#define CAP_PLAYERS 0x01
#define CAP_COINS 0x02
#define CAP_ANALOG_IN 0x03
#define CAP_ROTARY 0x04
#define CAP_KEYPAD 0x05
#define CAP_LIGHTGUN 0x06
#define CAP_GPI 0x07
#define CAP_CARD 0x10
#define CAP_HOPPER 0x11
#define CAP_GPO 0x12
#define CAP_ANALOG_OUT 0x13
#define CAP_DISPLAY 0x14
#define CAP_BACKUP 0x15

// Protocol Bytes
#define JVS_SYNC 0xE0
#define JVS_ESCAPE 0xD0
#define JVS_BROADCAST 0xFF
#define JVS_BUS_MASTER 0x00

// Status Reports
#define REPORT_SUCCESS 0x01
#define STATUS_SUCCESS 0x01

enum JVSInput {
    // System
    BUTTON_TEST = 1 << 7,
    BUTTON_SERVICE = 1 << 14,
    // Player
    BUTTON_START = 1 << 15,
    BUTTON_UP = 1 << 13,
    BUTTON_DOWN = 1 << 12,
    BUTTON_LEFT = 1 << 11,
    BUTTON_RIGHT = 1 << 10,
    BUTTON_1 = 1 << 9,
    BUTTON_2 = 1 << 8,
    BUTTON_3 = 1 << 7,
    BUTTON_4 = 1 << 6,
    BUTTON_5 = 1 << 5,
    BUTTON_6 = 1 << 4,
    // Analogue Channels
    ANALOGUE_1 = 0,
    ANALOGUE_2 = 1,
    ANALOGUE_3 = 2,
    ANALOGUE_4 = 3
};

enum JVSPlayer {
    SYSTEM = 0,
    PLAYER_1 = 1,
    PLAYER_2 = 2
};

struct JVSState {
    int coinCount[JVS_MAX_STATE_SIZE];
    int inputSwitch[JVS_MAX_STATE_SIZE];
    int analogueChannel[JVS_MAX_STATE_SIZE];
    int rotaryChannel[JVS_MAX_STATE_SIZE];
};

struct JVSCapabilities {
    char name[MAX_JVS_NAME_SIZE];
    unsigned char commandVersion;
    unsigned char jvsVersion;
    unsigned char commsVersion;
    unsigned char players;
    unsigned char switches;
    unsigned char coins;
    unsigned char analogueInChannels;
    unsigned char analogueInBits;
    unsigned char rotaryChannels;
    unsigned char keypad;
    unsigned char gunChannels;
    unsigned char gunXBits;
    unsigned char gunYBits;
    unsigned char generalPurposeInputs;
    unsigned char card;
    unsigned char hopper;
    unsigned char generalPurposeOutputs;
    unsigned char analogueOutChannels;
    unsigned char displayOutRows;
    unsigned char displayOutColumns;
    unsigned char displayOutEncodings;
    unsigned char backup;
    unsigned char rightAlignBits;
};

struct JVSPacket {
    unsigned char destination;
    unsigned char length;
    unsigned char data[JVS_MAX_PACKET_SIZE];
};

class JvsBoard {
public:
    JvsBoard();
    ~JvsBoard();

    bool Open();
    void Close();

    // The method called by BaseBoard
    void ProcessPacket(const uint8_t* input, size_t inputSize, uint8_t* output, int* outputSize);

    // Accessors
    int GetSenseLine() const { return m_senseLine; }
    void SetSenseLine(int sense) { m_senseLine = sense; }

    // Input Simulation (Thread-safe)
    void SetSwitch(JVSPlayer player, int switchMask, bool on);
    void IncrementCoin(JVSPlayer player, int amount);
    void SetAnalogue(int channel, int value);

private:
    void InitCapabilities(int type); // 0=Type1, 1=Type3

    // Packet helpers
    bool ReadPacket(const uint8_t* src, size_t srcLen);
    void WritePacket(uint8_t* dst, int* dstLen);
    void WriteFeature(JVSPacket* packet, char cap, char arg0, char arg1, char arg2);
    void WriteFeatures(JVSPacket* packet);

    // Internal state
    int m_senseLine;
    int m_deviceID;

    JVSState m_state;
    JVSCapabilities m_caps;

    // Working packets
    JVSPacket m_inputPacket;
    JVSPacket m_outputPacket;

    // Emulation Constants
    int m_analogueRestBits;
    int m_analogueMax;

    std::mutex m_mutex;
};
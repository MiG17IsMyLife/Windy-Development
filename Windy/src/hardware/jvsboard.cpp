#include "JvsBoard.h"
#include <iostream>

// JVS Commands
#define CMD_RESET 0xF0
#define CMD_ASSIGN_ADDR 0xF1
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
#define CMD_READ_GPI 0x25
#define CMD_REMAINING_PAYOUT 0x2E
#define CMD_WRITE_GPO 0x30
#define CMD_WRITE_GPO_BYTE 0x31
#define CMD_WRITE_GPO_BIT 0x32
#define CMD_SET_PAYOUT 0x32
#define CMD_SUBTRACT_PAYOUT 0x33
#define CMD_WRITE_ANALOG 0x34
#define CMD_WRITE_COINS 0x35
#define CMD_WRITE_DISPLAY 0x36
#define CMD_DECREASE_COINS 0x37

#define REPORT_SUCCESS 0x01

void JvsBoard::writeFeature(std::vector<uint8_t>& buf, uint8_t cap, uint8_t arg0, uint8_t arg1, uint8_t arg2) {
    buf.push_back(cap); buf.push_back(arg0); buf.push_back(arg1); buf.push_back(arg2);
}

void JvsBoard::ProcessPacket(const uint8_t* input, size_t inputSize, std::vector<uint8_t>& output) {
    output.clear();
    if (inputSize < 2) return;

    size_t index = 0;
    uint8_t dest = input[index++];
    uint8_t len = input[index++];

    if (dest != 0xFF && dest != deviceID && deviceID != -1) return;

    output.push_back(0xE0); // SYNC
    output.push_back(0x00); // BUS_MASTER
    output.push_back(0x00); // Length
    output.push_back(REPORT_SUCCESS);

    size_t cmdIndex = index;
    while (cmdIndex < inputSize && cmdIndex < (size_t)(len + 1)) {
        uint8_t cmd = input[cmdIndex++];

        switch (cmd) {
        case CMD_RESET:
            std::cout << "[JVS] CMD_RESET" << std::endl;
            deviceID = -1;
            break;
        case CMD_ASSIGN_ADDR:
            deviceID = input[cmdIndex++];
            std::cout << "[JVS] CMD_ASSIGN_ADDR: " << deviceID << std::endl;
            output.push_back(REPORT_SUCCESS);
            break;
        case CMD_REQUEST_ID: {
            std::cout << "[JVS] CMD_REQUEST_ID" << std::endl;
            const char* id = "SEGA ENTERPRISESLTD.;I/O BD JVS;837-13551;Ver1.00;98/10";
            output.push_back(REPORT_SUCCESS);
            for (size_t i = 0; i < strlen(id); i++) output.push_back(id[i]);
            output.push_back(0);
            break;
        }
        case CMD_COMMAND_VERSION:
            std::cout << "[JVS] CMD_COMMAND_VERSION" << std::endl;
            output.push_back(REPORT_SUCCESS); output.push_back(0x13);
            break;
        case CMD_JVS_VERSION:
            std::cout << "[JVS] CMD_JVS_VERSION" << std::endl;
            output.push_back(REPORT_SUCCESS); output.push_back(0x30);
            break;
        case CMD_COMMS_VERSION:
            std::cout << "[JVS] CMD_COMMS_VERSION" << std::endl;
            output.push_back(REPORT_SUCCESS); output.push_back(0x10);
            break;
        case CMD_CAPABILITIES:
            std::cout << "[JVS] CMD_CAPABILITIES" << std::endl;
            output.push_back(REPORT_SUCCESS);
            writeFeature(output, 1, 2, 14, 0); // Players=2, Switches=14
            writeFeature(output, 2, 2, 0, 0);  // Coins=2
            writeFeature(output, 3, 8, 10, 0); // Analog In=8ch, 10bits
            writeFeature(output, 0, 0, 0, 0);  // End
            break;
        case CMD_READ_SWITCHES: {
            // •po‚·‚é‚Ì‚ÅƒƒO‚ÍT‚¦‚ß‚É
            // std::cout << "[JVS] CMD_READ_SWITCHES" << std::endl;
            int players = input[cmdIndex++];
            int bytes = input[cmdIndex++];
            output.push_back(REPORT_SUCCESS);
            output.push_back(0); // System buttons
            for (int i = 0; i < players * bytes; i++) output.push_back(0);
            break;
        }
        case CMD_READ_COINS: {
            int slots = input[cmdIndex++];
            output.push_back(REPORT_SUCCESS);
            for (int i = 0; i < slots; i++) { output.push_back(0); output.push_back(0); }
            break;
        }
        case CMD_READ_ANALOGS: {
            int channels = input[cmdIndex++];
            output.push_back(REPORT_SUCCESS);
            for (int i = 0; i < channels; i++) { output.push_back(0x80); output.push_back(0x00); }
            break;
        }
        case CMD_READ_ROTARY: {
            int channels = input[cmdIndex++];
            output.push_back(REPORT_SUCCESS);
            for (int i = 0; i < channels; i++) { output.push_back(0); output.push_back(0); }
            break;
        }
        case CMD_WRITE_GPO: {
            int bytes = input[cmdIndex++];
            cmdIndex += bytes;
            output.push_back(REPORT_SUCCESS);
            break;
        }
        case CMD_WRITE_GPO_BIT:
            cmdIndex += 2;
            output.push_back(REPORT_SUCCESS);
            break;
        case CMD_CONVEY_ID: {
            std::cout << "[JVS] CMD_CONVEY_ID" << std::endl;
            while (cmdIndex < inputSize && input[cmdIndex] != 0) cmdIndex++;
            cmdIndex++;
            output.push_back(REPORT_SUCCESS);
            break;
        }
        default:
            std::cout << "[JVS] Unknown Command: 0x" << std::hex << (int)cmd << std::endl;
            break;
        }
    }

    if (output.size() > 2) output[2] = (uint8_t)(output.size() - 1);
}

int JvsBoard::Read(void* buf, size_t count) { return 0; }
int JvsBoard::Write(const void* buf, size_t count) { return (int)count; }
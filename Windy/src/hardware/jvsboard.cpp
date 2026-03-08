#define _CRT_SECURE_NO_WARNINGS

#include "jvsboard.h"
#include "../core/log.h"
#include <cstring>
#include <cmath>
#include <stdio.h>

JvsBoard::JvsBoard()
    : m_senseLine(3)
    , m_deviceID(-1)
    , m_ioType(SEGA_TYPE_3)
    , m_analogueRestBits(6)
    , m_analogueMax(1023)
{
    memset(&m_state, 0, sizeof(m_state));
    memset(&m_caps, 0, sizeof(m_caps));
    memset(&m_inputPacket, 0, sizeof(m_inputPacket));
    memset(&m_outputPacket, 0, sizeof(m_outputPacket));
}

JvsBoard::~JvsBoard() {
}

bool JvsBoard::Open() {
    log_info("JvsBoard::Open()");
    // Default to Type 3 (Lindbergh standard)
    InitCapabilities(SEGA_TYPE_3);
    return true;
}

void JvsBoard::Close() {
}

void JvsBoard::SetIOType(int type) {
    log_info("JvsBoard::SetIOType(%d)", type);
    m_ioType = type;
    InitCapabilities(type);
}

void JvsBoard::InitCapabilities(int type) {
    // Ported from initJVS() in Lindbergh-loader jvs.c
    memset(&m_caps, 0, sizeof(m_caps));

    if (type == SEGA_TYPE_1) {
        // After Burner Climax / SEGA Type 1 I/O
        m_caps.switches = 13;
        m_caps.coins = 2;
        m_caps.players = 2;
        m_caps.analogueInBits = 8;
        m_caps.rightAlignBits = 0;
        m_caps.analogueInChannels = 8;
        m_caps.generalPurposeOutputs = 6;
        m_caps.commandVersion = 17;  // 0x11
        m_caps.jvsVersion = 48;      // 0x30
        m_caps.commsVersion = 16;    // 0x10
        strcpy(m_caps.name, "SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551 ;Ver1.00;98/10");
        log_info("JvsBoard: Initialized as SEGA_TYPE_1 (After Burner Climax)");
    }
    else {
        // SEGA_TYPE_3 - Standard Lindbergh I/O
        m_caps.switches = 14;
        m_caps.coins = 2;
        m_caps.players = 2;
        m_caps.analogueInBits = 10;
        m_caps.rightAlignBits = 0;
        m_caps.analogueInChannels = 8;
        m_caps.generalPurposeOutputs = 20;
        m_caps.commandVersion = 19;  // 0x13
        m_caps.jvsVersion = 48;      // 0x30
        m_caps.commsVersion = 16;    // 0x10
        strcpy(m_caps.name, "SEGA CORPORATION;I/O BD JVS;837-14572;Ver1.00;2005/10");
        log_info("JvsBoard: Initialized as SEGA_TYPE_3 (Standard Lindbergh)");
    }

    // Calculate analog bit shifting
    if (!m_caps.rightAlignBits) {
        m_analogueRestBits = 16 - m_caps.analogueInBits;
    }
    else {
        m_analogueRestBits = 0;
    }

    m_analogueMax = (int)pow(2, m_caps.analogueInBits) - 1;

    // Center analogs (0x80 for 8-bit, 0x200 for 10-bit)
    int centerValue = m_analogueMax / 2;
    for (int i = 0; i < 8; ++i) {
        m_state.analogueChannel[i] = centerValue;
    }

    // Ready for connection
    m_senseLine = 3;
}

void JvsBoard::SetSwitch(JVSPlayer player, int switchMask, bool on) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (player > m_caps.players) return;

    if (on) {
        m_state.inputSwitch[player] |= switchMask;
    }
    else {
        m_state.inputSwitch[player] &= ~switchMask;
    }
}

void JvsBoard::IncrementCoin(JVSPlayer player, int amount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (player == SYSTEM) return;
    int idx = player - 1;
    if (idx >= 0 && idx < JVS_MAX_STATE_SIZE) {
        m_state.coinCount[idx] += amount;
        if (m_state.coinCount[idx] > 16383) m_state.coinCount[idx] = 16383;
        if (m_state.coinCount[idx] < 0) m_state.coinCount[idx] = 0;
    }
}

void JvsBoard::SetAnalogue(int channel, int value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (channel >= 0 && channel < JVS_MAX_STATE_SIZE) {
        // Clamp to max value
        if (value > m_analogueMax) value = m_analogueMax;
        if (value < 0) value = 0;
        m_state.analogueChannel[channel] = value;
    }
}

void JvsBoard::SetGun(int player, int x, int y) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (player >= 0 && player < 2) {
        m_state.gunChannel[player * 2] = x;
        m_state.gunChannel[player * 2 + 1] = y;
    }
}

void JvsBoard::SetRotary(int channel, int value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (channel >= 0 && channel < JVS_MAX_STATE_SIZE) {
        m_state.rotaryChannel[channel] = value;
    }
}

bool JvsBoard::ReadPacket(const uint8_t* src, size_t srcLen) {
    // Ported from readPacket in Lindbergh-loader jvs.c
    int escape = 0;
    int phase = 0;
    int dataIndex = 0;
    unsigned char checksum = 0x00;

    m_inputPacket.length = 0;

    for (size_t i = 0; i < srcLen; ++i) {
        uint8_t byte = src[i];

        if (!escape && byte == JVS_SYNC) {
            phase = 0;
            dataIndex = 0;
            checksum = 0;
            continue;
        }
        if (!escape && byte == JVS_ESCAPE) {
            escape = 1;
            continue;
        }
        if (escape) {
            byte++;
            escape = 0;
        }

        switch (phase) {
        case 0: // Destination
            m_inputPacket.destination = byte;
            checksum = (byte & 0xFF);
            phase++;
            break;
        case 1: // Length
            m_inputPacket.length = byte;
            checksum = (checksum + byte) & 0xFF;
            phase++;
            break;
        case 2: // Data
            if (dataIndex == m_inputPacket.length - 1) {
                // Checksum byte
                if (checksum != byte) {
                    log_warn("JvsBoard: Checksum Failure (expected 0x%02X, got 0x%02X)", checksum, byte);
                    //return false; Shitty fix.
                }
                return true; // Packet complete
            }
            if (dataIndex < JVS_MAX_PACKET_SIZE) {
                m_inputPacket.data[dataIndex++] = byte;
                checksum = (checksum + byte) & 0xFF;
            }
            break;
        }
    }
    return false;
}

void JvsBoard::WriteFeature(JVSPacket* packet, char cap, char arg0, char arg1, char arg2) {
    if (packet->length + 4 >= JVS_MAX_PACKET_SIZE) return;
    packet->data[packet->length++] = cap;
    packet->data[packet->length++] = arg0;
    packet->data[packet->length++] = arg1;
    packet->data[packet->length++] = arg2;
}

void JvsBoard::WriteFeatures(JVSPacket* packet) {
    packet->data[packet->length++] = REPORT_SUCCESS;

    if (m_caps.players)
        WriteFeature(packet, CAP_PLAYERS, m_caps.players, m_caps.switches, 0x00);
    if (m_caps.coins)
        WriteFeature(packet, CAP_COINS, m_caps.coins, 0x00, 0x00);
    if (m_caps.analogueInChannels)
        WriteFeature(packet, CAP_ANALOG_IN, m_caps.analogueInChannels, m_caps.analogueInBits, 0x00);
    if (m_caps.rotaryChannels)
        WriteFeature(packet, CAP_ROTARY, m_caps.rotaryChannels, 0x00, 0x00);
    if (m_caps.keypad)
        WriteFeature(packet, CAP_KEYPAD, 0x00, 0x00, 0x00);
    if (m_caps.gunChannels)
        WriteFeature(packet, CAP_LIGHTGUN, m_caps.gunXBits, m_caps.gunYBits, m_caps.gunChannels);
    if (m_caps.generalPurposeInputs)
        WriteFeature(packet, CAP_GPI, 0x00, m_caps.generalPurposeInputs, 0x00);
    if (m_caps.card)
        WriteFeature(packet, CAP_CARD, m_caps.card, 0x00, 0x00);
    if (m_caps.hopper)
        WriteFeature(packet, CAP_HOPPER, m_caps.hopper, 0x00, 0x00);
    if (m_caps.generalPurposeOutputs)
        WriteFeature(packet, CAP_GPO, m_caps.generalPurposeOutputs, 0x00, 0x00);
    if (m_caps.analogueOutChannels)
        WriteFeature(packet, CAP_ANALOG_OUT, m_caps.analogueOutChannels, 0x00, 0x00);
    if (m_caps.displayOutColumns)
        WriteFeature(packet, CAP_DISPLAY, m_caps.displayOutColumns, m_caps.displayOutRows, m_caps.displayOutEncodings);
    if (m_caps.backup)
        WriteFeature(packet, CAP_BACKUP, 0x00, 0x00, 0x00);

    packet->data[packet->length++] = CAP_END;
}

void JvsBoard::WritePacket(uint8_t* dst, int* dstLen) {
    // Ported from writePacket in Lindbergh-loader jvs.c
    m_outputPacket.length++; // Account for checksum byte in protocol

    int checksum = 0;
    int outIdx = 0;

    dst[outIdx++] = JVS_SYNC;

    // Helper lambda for escaping
    auto writeByte = [&](uint8_t b) {
        if (b == JVS_SYNC || b == JVS_ESCAPE) {
            dst[outIdx++] = JVS_ESCAPE;
            dst[outIdx++] = b - 1;
        }
        else {
            dst[outIdx++] = b;
        }
        checksum = (checksum + b) & 0xFF;
        };

    // Destination
    writeByte(m_outputPacket.destination);

    // Length
    writeByte(m_outputPacket.length);

    // Data
    for (int i = 0; i < m_outputPacket.length - 1; i++) {
        writeByte(m_outputPacket.data[i]);
    }

    // Checksum (write with escape if needed)
    if (checksum == JVS_SYNC || checksum == JVS_ESCAPE) {
        dst[outIdx++] = JVS_ESCAPE;
        dst[outIdx++] = (uint8_t)(checksum - 1);
    }
    else {
        dst[outIdx++] = (uint8_t)checksum;
    }

    *dstLen = outIdx;
}

void JvsBoard::ProcessPacket(const uint8_t* input, size_t inputSize, uint8_t* output, int* outputSize) {
    if (!ReadPacket(input, inputSize)) {
        *outputSize = 0;
        return;
    }

    // Check destination
    if (m_inputPacket.destination != JVS_BROADCAST && m_inputPacket.destination != m_deviceID) {
        // Not for us
        *outputSize = 0;
        return;
    }

    // Prepare Output
    m_outputPacket.length = 0;
    m_outputPacket.destination = JVS_BUS_MASTER;

    // Status Success
    m_outputPacket.data[m_outputPacket.length++] = STATUS_SUCCESS;

    std::lock_guard<std::mutex> lock(m_mutex);

    int index = 0;
    // Loop through commands in the packet
    // inputPacket.length includes the checksum byte, so iterate until length - 1
    while (index < m_inputPacket.length - 1) {
        uint8_t cmd = m_inputPacket.data[index++];
        int size = 0; // Bytes consumed (args)

        switch (cmd) {
        case CMD_RESET:
            size = 1; // 0xD9 follows
            m_deviceID = -1;
            m_senseLine = 3;
            break;

        case CMD_ASSIGN_ADDR:
            size = 1;
            m_deviceID = m_inputPacket.data[index];
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            m_senseLine = 1;
            break;

        case CMD_REQUEST_ID:
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            {
                size_t len = strlen(m_caps.name);
                memcpy(&m_outputPacket.data[m_outputPacket.length], m_caps.name, len + 1);
                m_outputPacket.length += (unsigned char)(len + 1);
            }
            break;

        case CMD_COMMAND_VERSION:
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            m_outputPacket.data[m_outputPacket.length++] = m_caps.commandVersion;
            break;

        case CMD_JVS_VERSION:
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            m_outputPacket.data[m_outputPacket.length++] = m_caps.jvsVersion;
            break;

        case CMD_COMMS_VERSION:
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            m_outputPacket.data[m_outputPacket.length++] = m_caps.commsVersion;
            break;

        case CMD_CAPABILITIES:
            WriteFeatures(&m_outputPacket);
            break;

        case CMD_READ_SWITCHES:
        {
            int numPlayers = m_inputPacket.data[index];
            int bytesPer = m_inputPacket.data[index + 1];
            size = 2;

            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            m_outputPacket.data[m_outputPacket.length++] = (unsigned char)m_state.inputSwitch[0]; // System

            for (int i = 0; i < numPlayers; i++) {
                int pVal = m_state.inputSwitch[i + 1];
                for (int j = 0; j < bytesPer; j++) {
                    // Shift out bytes, MSB first
                    int shift = 8 - (j * 8);
                    if (shift < 0) shift = 0;
                    m_outputPacket.data[m_outputPacket.length++] = (pVal >> shift) & 0xFF;
                }
            }
        }
        break;

        case CMD_READ_COINS:
        {
            int slots = m_inputPacket.data[index];
            size = 1;
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            for (int i = 0; i < slots; i++) {
                int val = m_state.coinCount[i];
                m_outputPacket.data[m_outputPacket.length++] = (val >> 8) & 0x3F;
                m_outputPacket.data[m_outputPacket.length++] = val & 0xFF;
            }
        }
        break;

        case CMD_READ_ANALOGS:
        {
            int channels = m_inputPacket.data[index];
            size = 1;
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            for (int i = 0; i < channels; i++) {
                int val = m_state.analogueChannel[i];
                // Apply alignment
                val = val << m_analogueRestBits;
                m_outputPacket.data[m_outputPacket.length++] = (val >> 8) & 0xFF;
                m_outputPacket.data[m_outputPacket.length++] = val & 0xFF;
            }
        }
        break;

        case CMD_READ_ROTARY:
        {
            int channels = m_inputPacket.data[index];
            size = 1;
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            for (int i = 0; i < channels; i++) {
                int val = m_state.rotaryChannel[i];
                m_outputPacket.data[m_outputPacket.length++] = (val >> 8) & 0xFF;
                m_outputPacket.data[m_outputPacket.length++] = val & 0xFF;
            }
        }
        break;

        case CMD_READ_LIGHTGUN:
        {
            int channels = m_inputPacket.data[index];
            size = 1;
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            for (int i = 0; i < channels; i++) {
                int x = m_state.gunChannel[i * 2];
                int y = m_state.gunChannel[i * 2 + 1];
                m_outputPacket.data[m_outputPacket.length++] = (x >> 8) & 0xFF;
                m_outputPacket.data[m_outputPacket.length++] = x & 0xFF;
                m_outputPacket.data[m_outputPacket.length++] = (y >> 8) & 0xFF;
                m_outputPacket.data[m_outputPacket.length++] = y & 0xFF;
            }
        }
        break;

        case CMD_READ_KEYPAD:
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            m_outputPacket.data[m_outputPacket.length++] = 0;
            break;

        case CMD_READ_GPI:
        {
            int count = m_inputPacket.data[index];
            size = 1;
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            for (int i = 0; i < count; i++) {
                m_outputPacket.data[m_outputPacket.length++] = 0;
            }
        }
        break;

        case CMD_WRITE_GPO:
            size = 1 + m_inputPacket.data[index]; // Arg byte + data bytes
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            break;

        case CMD_WRITE_ANALOG:
            size = (m_inputPacket.data[index] * 2) + 1;
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            break;

        case CMD_WRITE_COINS:
        {
            // Slot index [index], Amount MSB [index+1], Amount LSB [index+2]
            int slot = m_inputPacket.data[index] - 1;
            int amount = (m_inputPacket.data[index + 1] << 8) | m_inputPacket.data[index + 2];
            size = 3;

            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
            if (slot >= 0 && slot < JVS_MAX_STATE_SIZE) {
                m_state.coinCount[slot] += amount;
                if (m_state.coinCount[slot] > 16383) m_state.coinCount[slot] = 16383;
            }
        }
        break;

        case CMD_DECREASE_COINS:
        {
            int slot = m_inputPacket.data[index] - 1;
            int amount = (m_inputPacket.data[index + 1] << 8) | m_inputPacket.data[index + 2];
            size = 3;
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;

            if (slot >= 0 && slot < JVS_MAX_STATE_SIZE) {
                m_state.coinCount[slot] -= amount;
                if (m_state.coinCount[slot] < 0) m_state.coinCount[slot] = 0;
            }
        }
        break;

        case CMD_CONVEY_ID:
            // String follows
        {
            int k = index;
            while (m_inputPacket.data[k] != 0 && k < m_inputPacket.length) {
                k++;
            }
            size = (k - index) + 1; // +1 for null
            m_outputPacket.data[m_outputPacket.length++] = REPORT_SUCCESS;
        }
        break;

        default:
            log_warn("JvsBoard: Unknown CMD 0x%02X", cmd);
            break;
        }

        index += size;
    }

    WritePacket(output, outputSize);
}
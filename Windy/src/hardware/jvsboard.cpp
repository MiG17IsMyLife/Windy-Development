#include "JvsBoard.h"
#include "../src/core/log.h"
#include <cstring>
#include <windows.h>
#include <math.h>

JvsBoard::JvsBoard() {
    m_address = 0;
    m_senseLine = false; // Initially disconnected until Reset/Assign
    m_coinCount[0] = 0;
    m_coinCount[1] = 0;
    m_coinPressed = false;

    // Setup Capabilities (SEGA TYPE 3 emulation from lindbergh-loader)
    m_caps.players = 2;
    m_caps.switches = 14;
    m_caps.coins = 2;
    m_caps.analogueInChannels = 8;
    m_caps.analogueInBits = 10;
    m_caps.rotaryChannels = 0;
    m_caps.keypad = 0;
    m_caps.gunChannels = 0;
    m_caps.generalPurposeInputs = 0;
    m_caps.card = 0;
    m_caps.hopper = 0;
    m_caps.generalPurposeOutputs = 20;
    m_caps.analogueOutChannels = 0;
    m_caps.displayOutRows = 0;
    m_caps.displayOutColumns = 0;
    m_caps.backup = 0;
    m_caps.name = "SEGA CORPORATION;I/O BD JVS;837-14572;Ver1.00;2005/10";
}

JvsBoard::~JvsBoard() {
}

bool JvsBoard::Open() {
    log_info("JvsBoard::Open()");
    m_inputBuffer.reserve(1024);
    m_outputBuffer.reserve(1024);
    return true;
}

void JvsBoard::Close() {
    log_debug("JvsBoard::Close()");
}

int JvsBoard::Read(void* buf, size_t count) {
    if (m_outputBuffer.empty()) return 0;

    size_t copySize = count;
    if (copySize > m_outputBuffer.size()) {
        copySize = m_outputBuffer.size();
    }

    memcpy(buf, m_outputBuffer.data(), copySize);

    // Remove read data from buffer
    m_outputBuffer.erase(m_outputBuffer.begin(), m_outputBuffer.begin() + copySize);

    return (int)copySize;
}

int JvsBoard::Write(const void* buf, size_t count) {
    const uint8_t* bytes = (const uint8_t*)buf;

    // Accumulate raw bytes
    for (size_t i = 0; i < count; i++) {
        uint8_t b = bytes[i];

        // SYNC detected, reset buffer
        if (b == JVS_SYNC) {
            m_inputBuffer.clear();
        }

        m_inputBuffer.push_back(b);
    }

    // Try to process packet if we have enough data
    // Format: [SYNC][DEST][LEN][DATA...][SUM]
    // Minimum: SYNC, DEST, LEN, SUM (4 bytes)
    if (m_inputBuffer.size() >= 4) {
        // Handle escaping to calculate real length
        // In this simple emulation, we'll try to process once we have at least 'LEN' bytes + header
        // Note: Real JVS requires careful unescaping first.

        // Quick parse for unescaped packets (standard for emulators usually)
        uint8_t len = m_inputBuffer[2];
        if (m_inputBuffer.size() >= (size_t)(3 + len)) {
            // Unescape packet
            std::vector<uint8_t> packet;
            bool escape = false;
            for (size_t i = 0; i < m_inputBuffer.size(); i++) {
                if (m_inputBuffer[i] == JVS_SYNC && i == 0) continue; // Skip SYNC
                if (m_inputBuffer[i] == JVS_ESCAPE) {
                    escape = true;
                    continue;
                }
                if (escape) {
                    packet.push_back(m_inputBuffer[i] + 1);
                    escape = false;
                }
                else {
                    packet.push_back(m_inputBuffer[i]);
                }
            }

            // Now packet contains [DEST][LEN][DATA...][SUM]
            if (packet.size() >= 2 && packet.size() >= (size_t)(packet[1] + 1)) {
                ProcessPacket(packet);
                m_inputBuffer.clear(); // Consumed
            }
        }
    }

    return (int)count;
}

int JvsBoard::Ioctl(unsigned long request, void* data) {
    return -1;
}

void JvsBoard::WriteFeature(std::vector<uint8_t>& out, uint8_t cap, uint8_t arg1, uint8_t arg2, uint8_t arg3) {
    out.push_back(cap);
    out.push_back(arg1);
    out.push_back(arg2);
    out.push_back(arg3);
}

void JvsBoard::ProcessPacket(const std::vector<uint8_t>& packet) {
    // packet: [DEST][LEN][CMD...][SUM]
    uint8_t dest = packet[0];

    // Check if packet is for us (or broadcast)
    if (dest != JVS_BROADCAST && dest != m_address) {
        return;
    }

    std::vector<uint8_t> response;
    response.push_back(JVS_REPORT_SUCCESS); // Overall Status

    size_t cursor = 2; // Skip DEST, LEN
    size_t end = packet.size() - 1; // Skip SUM

    while (cursor < end) {
        uint8_t cmd = packet[cursor++];

        switch (cmd) {
        case CMD_RESET: {
            cursor++; // Skip arg (0xD9)
            m_address = 0;
            m_senseLine = false; // Reset sense
            log_info("JvsBoard: CMD_RESET");
            // No response data for reset usually, or standard header
            break;
        }

        case CMD_ASSIGN_ADDR: {
            uint8_t newAddr = packet[cursor++];
            m_address = newAddr;
            m_senseLine = true; // Address assigned, ready
            response.push_back(JVS_REPORT_SUCCESS);
            log_info("JvsBoard: CMD_ASSIGN_ADDR -> 0x%02X", newAddr);
            break;
        }

        case CMD_REQUEST_ID: {
            response.push_back(JVS_REPORT_SUCCESS);
            const char* id = m_caps.name.c_str();
            while (*id) response.push_back((uint8_t)*id++);
            response.push_back(0); // Null terminator
            log_debug("JvsBoard: CMD_REQUEST_ID");
            break;
        }

        case CMD_COMMAND_VERSION:
            response.push_back(JVS_REPORT_SUCCESS);
            response.push_back(0x13); // Ver 1.3
            break;

        case CMD_JVS_VERSION:
            response.push_back(JVS_REPORT_SUCCESS);
            response.push_back(0x30); // Ver 3.0
            break;

        case CMD_COMMS_VERSION:
            response.push_back(JVS_REPORT_SUCCESS);
            response.push_back(0x10); // Ver 1.0
            break;

        case CMD_CAPABILITIES: {
            log_debug("JvsBoard: CMD_CAPABILITIES");
            response.push_back(JVS_REPORT_SUCCESS);

            if (m_caps.players) WriteFeature(response, 1, m_caps.players, m_caps.switches, 0);
            if (m_caps.coins)   WriteFeature(response, 2, m_caps.coins, 0, 0);
            if (m_caps.analogueInChannels) WriteFeature(response, 3, m_caps.analogueInChannels, m_caps.analogueInBits, 0);
            if (m_caps.generalPurposeOutputs) WriteFeature(response, 0x12, m_caps.generalPurposeOutputs, 0, 0);

            response.push_back(0); // End
            break;
        }

        case CMD_READ_SWITCHES: {
            uint8_t numPlayers = packet[cursor++];
            uint8_t bytesPerPlayer = packet[cursor++];

            response.push_back(JVS_REPORT_SUCCESS);

            // --- Input Polling (GetAsyncKeyState) ---

            // System Byte: [TEST][0][0][0] [0][0][0][SERVICE]
            uint8_t sys = 0;
            if (GetAsyncKeyState('T') & 0x8000) sys |= SW_TEST;
            if (GetAsyncKeyState('S') & 0x8000) sys |= SW_SERVICE;
            response.push_back(sys);

            // Player 1
            if (numPlayers >= 1) {
                uint8_t p1_1 = 0;
                if (GetAsyncKeyState('1') & 0x8000)     p1_1 |= SW_START;
                if (GetAsyncKeyState(VK_UP) & 0x8000)    p1_1 |= SW_UP;
                if (GetAsyncKeyState(VK_DOWN) & 0x8000)  p1_1 |= SW_DOWN;
                if (GetAsyncKeyState(VK_LEFT) & 0x8000)  p1_1 |= SW_LEFT;
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000) p1_1 |= SW_RIGHT;
                if (GetAsyncKeyState('Z') & 0x8000)      p1_1 |= SW_BTN1;
                if (GetAsyncKeyState('X') & 0x8000)      p1_1 |= SW_BTN2;
                response.push_back(p1_1);

                if (bytesPerPlayer > 1) {
                    uint8_t p1_2 = 0;
                    if (GetAsyncKeyState('C') & 0x8000)  p1_2 |= SW_BTN3;
                    if (GetAsyncKeyState('V') & 0x8000)  p1_2 |= SW_BTN4;
                    response.push_back(p1_2);
                }
            }

            // Player 2 (Stubbed)
            if (numPlayers >= 2) {
                response.push_back(0);
                if (bytesPerPlayer > 1) response.push_back(0);
            }
            break;
        }

        case CMD_READ_COINS: {
            uint8_t slots = packet[cursor++];
            response.push_back(JVS_REPORT_SUCCESS);

            // Coin Logic
            bool coinNow = (GetAsyncKeyState('5') & 0x8000) != 0;
            if (coinNow && !m_coinPressed) {
                m_coinCount[0]++;
                log_info("JvsBoard: Coin Inserted (Total: %d)", m_coinCount[0]);
            }
            m_coinPressed = coinNow;

            // Slot 1 (2 bytes, Big Endian)
            response.push_back((m_coinCount[0] >> 8) & 0xFF);
            response.push_back(m_coinCount[0] & 0xFF);

            // Slot 2
            if (slots > 1) {
                response.push_back(0);
                response.push_back(0);
            }
            break;
        }

        case CMD_READ_ANALOGS: {
            uint8_t channels = packet[cursor++];
            response.push_back(JVS_REPORT_SUCCESS);

            // Channel 0: Steering (0x0000 - 0xFFFF)
            // Left=0000, Center=8000, Right=FFFF
            uint16_t steering = 0x8000;
            if (GetAsyncKeyState(VK_LEFT) & 0x8000)  steering = 0x0000;
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) steering = 0xFFFF;

            response.push_back((steering >> 8) & 0xFF);
            response.push_back(steering & 0xFF);

            // Channel 1: Gas (0000-FFFF)
            uint16_t gas = 0;
            if (GetAsyncKeyState(VK_UP) & 0x8000) gas = 0xFFFF;
            response.push_back((gas >> 8) & 0xFF);
            response.push_back(gas & 0xFF);

            // Channel 2: Brake
            uint16_t brake = 0;
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) brake = 0xFFFF;
            response.push_back((brake >> 8) & 0xFF);
            response.push_back(brake & 0xFF);

            // Fill rest
            for (int i = 3; i < channels; i++) {
                response.push_back(0);
                response.push_back(0);
            }
            break;
        }

        case CMD_WRITE_GPO: {
            uint8_t len = packet[cursor++];
            cursor += len; // Skip data
            response.push_back(JVS_REPORT_SUCCESS);
            break;
        }

        default:
            log_warn("JvsBoard: Unhandled CMD 0x%02X", cmd);
            response.push_back(JVS_STATUS_UNKNOWN_CMD);
            // Rough estimation to skip arguments is hard without a table, 
            // but loop might catch next command or fail. 
            // For now assume single byte or exit.
            break;
        }
    }

    WritePacket(response);
}

void JvsBoard::WritePacket(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> packet;
    packet.push_back(JVS_SYNC);
    packet.push_back(JVS_BUS_MASTER); // Dest
    packet.push_back((uint8_t)(data.size() + 1)); // Len includes SUM

    // Escaping
    uint8_t sum = JVS_BUS_MASTER + (uint8_t)(data.size() + 1);

    for (uint8_t b : data) {
        sum = (sum + b) & 0xFF;
        if (b == JVS_SYNC || b == JVS_ESCAPE) {
            packet.push_back(JVS_ESCAPE);
            packet.push_back(b - 1);
        }
        else {
            packet.push_back(b);
        }
    }

    // Write Sum
    if (sum == JVS_SYNC || sum == JVS_ESCAPE) {
        packet.push_back(JVS_ESCAPE);
        packet.push_back(sum - 1);
    }
    else {
        packet.push_back(sum);
    }

    // Append to Output Buffer
    m_outputBuffer.insert(m_outputBuffer.end(), packet.begin(), packet.end());
}
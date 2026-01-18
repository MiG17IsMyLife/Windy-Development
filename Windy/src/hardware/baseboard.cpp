#define NOMINMAX 

#include "BaseBoard.h"
#include <iostream>
#include <algorithm>

#define BASEBOARD_INIT 0x300
#define BASEBOARD_GET_VERSION 0x8004BC02
#define BASEBOARD_SEEK_SHM 0x400
#define BASEBOARD_READ_SRAM 0x601
#define BASEBOARD_WRITE_SRAM 0x600
#define BASEBOARD_REQUEST 0xC020BC06
#define BASEBOARD_RECEIVE 0xC020BC07
#define BASEBOARD_GET_SERIAL 0x120
#define BASEBOARD_WRITE_FLASH 0x180
#define BASEBOARD_GET_SENSE_LINE 0x210
#define BASEBOARD_PROCESS_JVS 0x220
#define BASEBOARD_READY 0x201

BaseBoard::BaseBoard() {
    sharedMemory.resize(1024 * 1024, 0);
    sram.resize(1024 * 128, 0);
}

int BaseBoard::Ioctl(unsigned long request, void* data) {
    // Just logging
    std::cout << "[BaseBoard] Ioctl Req: 0x" << std::hex << request << std::dec << std::endl;

    switch (request) {
    case BASEBOARD_GET_VERSION: {
        std::cout << "[BaseBoard] GET_VERSION" << std::endl;
        uint8_t version[] = { 0x00, 0x19, 0x20, 0x07 };
        memcpy(data, version, 4);
        return 0;
    }
    case BASEBOARD_SEEK_SHM:
        // Another logging
        // std::cout << "[BaseBoard] SEEK_SHM" << std::endl;
        sharedMemoryIndex = (unsigned int)(uintptr_t)data;
        return 0;
    case BASEBOARD_READ_SRAM: {
        std::cout << "[BaseBoard] READ_SRAM" << std::endl;
        uint32_t* args = (uint32_t*)data;
        uint32_t offset = args[1];
        uint32_t size = args[2];
        void* dest = (void*)args[0];
        if (offset + size <= sram.size()) memcpy(dest, &sram[offset], size);
        return 0;
    }
    case BASEBOARD_WRITE_SRAM: {
        std::cout << "[BaseBoard] WRITE_SRAM" << std::endl;
        uint32_t* args = (uint32_t*)data;
        uint32_t offset = args[0];
        uint32_t size = args[2];
        void* src = (void*)args[1];
        if (offset + size <= sram.size()) memcpy(&sram[offset], src, size);
        return 0;
    }
    case BASEBOARD_REQUEST: {
        uint32_t* cmd = (uint32_t*)data;
        uint32_t commandID = cmd[0];

        // Logging each commands
        switch (commandID) {
        case BASEBOARD_GET_SERIAL: std::cout << "[BaseBoard] REQUEST: GET_SERIAL" << std::endl; break;
        case BASEBOARD_PROCESS_JVS: std::cout << "[BaseBoard] REQUEST: PROCESS_JVS" << std::endl; break;
        default: std::cout << "[BaseBoard] REQUEST: Unknown (0x" << std::hex << commandID << ")" << std::endl; break;
        }

        switch (commandID) {
        case BASEBOARD_GET_SERIAL:
            serialCommand.destAddress = cmd[1];
            serialCommand.destSize = cmd[2];
            break;
        case BASEBOARD_PROCESS_JVS:
            jvsCommand.srcAddress = cmd[1];
            jvsCommand.srcSize = cmd[2];
            jvsCommand.destAddress = cmd[3];
            jvsCommand.destSize = cmd[4];
            break;
        }
        cmd[0] |= 0xF0000000;
        return 0;
    }
    case BASEBOARD_RECEIVE: {
        uint32_t* cmd = (uint32_t*)data;
        uint32_t commandID = cmd[0] & 0xFFF;

        switch (commandID) {
        case BASEBOARD_GET_SERIAL: {
            const char* serial = "FE11-X018012022X";
            if (serialCommand.destAddress + 96 < sharedMemory.size()) {
                memcpy(&sharedMemory[serialCommand.destAddress + 96], serial, strlen(serial));
            }
            cmd[1] = 1;
            break;
        }
        case BASEBOARD_GET_SENSE_LINE:            
            std::cout << "[BaseBoard] RECEIVE: GET_SENSE_LINE (Returning 1)" << std::endl;
            cmd[2] = 1; cmd[1] = 1;
            break;
        case BASEBOARD_PROCESS_JVS: {
            std::cout << "[BaseBoard] RECEIVE: PROCESS_JVS" << std::endl;
            std::vector<uint8_t> response;
            if (jvsCommand.srcAddress < sharedMemory.size()) {
                jvs.ProcessPacket(&sharedMemory[jvsCommand.srcAddress], jvsCommand.srcSize, response);
            }
            if (!response.empty() && jvsCommand.destAddress < sharedMemory.size()) {
                size_t copySize = (std::min)(response.size(), (size_t)jvsCommand.destSize);
                memcpy(&sharedMemory[jvsCommand.destAddress], response.data(), copySize);
                cmd[3] = copySize;
            }
            else { cmd[3] = 0; }
            cmd[2] = jvsCommand.destAddress;
            cmd[1] = 1;
            break;
        }
        default:
            std::cout << "[BaseBoard] RECEIVE: Unknown ID 0x" << std::hex << commandID << std::endl;
            break;
        }
        cmd[0] |= 0xF0000000;
        return 0;
    }
    }
    return 0;
}

int BaseBoard::Read(void* buf, size_t count) {
    if (sharedMemoryIndex + count > sharedMemory.size()) count = sharedMemory.size() - sharedMemoryIndex;
    memcpy(buf, &sharedMemory[sharedMemoryIndex], count);
    return (int)count;
}

int BaseBoard::Write(const void* buf, size_t count) {
    if (sharedMemoryIndex + count > sharedMemory.size()) count = sharedMemory.size() - sharedMemoryIndex;
    memcpy(&sharedMemory[sharedMemoryIndex], buf, count);
    return (int)count;
}
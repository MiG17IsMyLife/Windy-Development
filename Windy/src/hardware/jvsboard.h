#pragma once
#include "LindberghDevice.h"
#include <vector>

class JvsBoard : public LindberghDevice {
private:
    std::vector<uint8_t> outputBuffer; // send buffer
    int deviceID = -1;                 // JVS address

    // JVS info feature writer
    void writeFeature(std::vector<uint8_t>& buf, uint8_t cap, uint8_t arg0, uint8_t arg1, uint8_t arg2);

public:
    // pakcet processor
    void ProcessPacket(const uint8_t* input, size_t inputSize, std::vector<uint8_t>& output);

    // LindberghDevice implementions
    int Read(void* buf, size_t count) override;
    int Write(const void* buf, size_t count) override;
};
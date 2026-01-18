#pragma once
#include "LindberghDevice.h"
#include <vector>

// TODO: Add baseboard implementions from original Lindbergh-Loader
class JvsBoard : public LindberghDevice {
public:
    JvsBoard();
    virtual ~JvsBoard();

    // LindberghDevice overrides
    bool Open() override;
    void Close() override;
    int Read(void* buf, size_t count) override;
    int Write(const void* buf, size_t count) override;
    int Ioctl(unsigned long request, void* data) override;

    // JVS specific
    void ProcessPacket(const unsigned char* data, unsigned int len, std::vector<unsigned char>& reply);
};
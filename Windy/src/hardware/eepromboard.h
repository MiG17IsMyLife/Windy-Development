#pragma once
#include "lindberghdevice.h"

class EepromBoard : public LindberghDevice {
public:
    int Ioctl(unsigned long request, void* data) override;
};
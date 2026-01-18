#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <algorithm>

class LindberghDevice {
public:
    virtual ~LindberghDevice() {}
    virtual bool Open() { return true; }
    virtual int Ioctl(unsigned long request, void* data) { return 0; }
    virtual int Read(void* buf, size_t count) { return 0; }
    virtual int Write(const void* buf, size_t count) { return 0; }
};
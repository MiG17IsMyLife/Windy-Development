#pragma once

#include <stdint.h>
#include <stddef.h>

class SerialBoard {
public:
    SerialBoard();
    ~SerialBoard();

    bool Open();
    void Close();
    int Ioctl(unsigned long request, void* data);
    int Read(void* buf, size_t count);
    int Write(const void* buf, size_t count);

private:
};
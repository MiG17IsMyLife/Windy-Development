#include "BaseBoard.h"
#include <iostream>

// TODO: Add baseboard implementions from original Lindbergh-Loader

bool BaseBoard::Open() {
    return true;
}

void BaseBoard::Close() {
}

int BaseBoard::Read(void* buf, size_t count) {
    return 0;
}

int BaseBoard::Write(const void* buf, size_t count) {
    return (int)count;
}

int BaseBoard::Ioctl(unsigned long request, void* data) {
    return 0;
}
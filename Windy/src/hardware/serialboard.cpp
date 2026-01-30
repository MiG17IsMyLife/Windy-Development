#include "serialboard.h"
#include "../core/log.h"

SerialBoard::SerialBoard() {
}

SerialBoard::~SerialBoard() {
}

bool SerialBoard::Open() {
    log_debug("SerialBoard: Open (stub)");
    return true;
}

void SerialBoard::Close() {
    log_debug("SerialBoard: Close (stub)");
}

int SerialBoard::Read(char* buf, unsigned int count) {
    (void)buf;
    (void)count;
    return 0;
}

int SerialBoard::Write(const char* buf, unsigned int count) {
    (void)buf;
    (void)count;
    return (int)count;
}

int SerialBoard::Ioctl(unsigned int request, void* data) {
    (void)request;
    (void)data;
    return 0;
}
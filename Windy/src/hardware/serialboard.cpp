#include "SerialBoard.h"
#include "../src/core/log.h"

// Linux Termios IOCTLs
#define TCGETS   0x5401
#define TCSETS   0x5402
#define TCSETSW  0x5403
#define TCSETSF  0x5404
#define TIOCMGET 0x5415
#define TIOCMSET 0x5418

SerialBoard::SerialBoard() {}
SerialBoard::~SerialBoard() {}

bool SerialBoard::Open() {
    log_info("SerialBoard: Open (Dummy for Server Box)");
    return true;
}

void SerialBoard::Close() {
    log_debug("SerialBoard: Close");
}

int SerialBoard::Read(void* buf, size_t count) {

    return 0;
}

int SerialBoard::Write(const void* buf, size_t count) {
    return (int)count;
}

int SerialBoard::Ioctl(unsigned long request, void* data) {
    switch (request) {
    case TCGETS:
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
    case TIOCMGET:
    case TIOCMSET:
        return 0;
    default:
        log_warn("SerialBoard: Unknown Ioctl 0x%lX", request);
        return 0;
    }
}
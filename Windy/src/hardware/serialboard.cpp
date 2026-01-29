#include "serialboard.h"
#include "../core/log.h"

SerialBoard::SerialBoard() {
}

SerialBoard::~SerialBoard() {
}

bool SerialBoard::Open() {
    log_info("SerialBoard: Open");
    return true;
}

void SerialBoard::Close() {
}

int SerialBoard::Ioctl(unsigned long request, void* data) {
    // 必要なIOCTL処理があればここに記述
    return 0;
}

int SerialBoard::Read(void* buf, size_t count) {
    return 0;
}

int SerialBoard::Write(const void* buf, size_t count) {
    return 0;
}
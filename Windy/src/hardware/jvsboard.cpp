#include "JvsBoard.h"

// TODO: Add baseboard implementions from original Lindbergh-Loader
JvsBoard::JvsBoard() {}
JvsBoard::~JvsBoard() {}

bool JvsBoard::Open() {
    return true;
}

void JvsBoard::Close() {
}

int JvsBoard::Read(void* buf, size_t count) {
    return 0;
}

int JvsBoard::Write(const void* buf, size_t count) {
    return (int)count;
}

int JvsBoard::Ioctl(unsigned long request, void* data) {
    return 0;
}

void JvsBoard::ProcessPacket(const unsigned char* data, unsigned int len, std::vector<unsigned char>& reply) {

}
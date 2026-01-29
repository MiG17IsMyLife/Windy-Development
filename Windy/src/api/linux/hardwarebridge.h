#pragma once

#include <stdint.h>
#include <stddef.h>

// LinuxTypes.h で定義されている iovec 等を使用します
#include "LinuxTypes.h"

extern "C" {
    // ファイルI/OおよびLindberghハードウェアアクセス
    int my_open(const char* pathname, int flags, int mode);
    int my_close(int fd);
    int my_read(int fd, void* buf, size_t count);
    int my_write(int fd, const void* buf, size_t count);
    int my_ioctl(int fd, unsigned long request, void* data);
    int my_writev(int fd, const struct iovec* iov, int iovcnt);

    // ポートI/Oエミュレーション (SecurityBoard等で使用)
    int HardwarePortRead(uint16_t port, uint32_t* data);
    int HardwarePortWrite(uint16_t port, uint32_t data);
}
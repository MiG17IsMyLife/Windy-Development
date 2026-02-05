#pragma once

#include <stdint.h>
#include <stddef.h>

#include "linuxtypes.h"

void InitHardwareBridge();
void CleanupHardwareBridge();

extern "C"
{
    int my_open(const char *pathname, int flags, int mode);
    int my_close(int fd);
    int my_read(int fd, void *buf, size_t count);
    int my_write(int fd, const void *buf, size_t count);
    int my_ioctl(int fd, unsigned long request, void *data);
    int my_writev(int fd, const struct iovec *iov, int iovcnt);

    // Port I/O emulation
    int HardwarePortRead(uint16_t port, uint32_t *data);
    int HardwarePortWrite(uint16_t port, uint32_t data);
}
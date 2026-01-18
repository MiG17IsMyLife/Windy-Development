#pragma once

#include "LinuxTypes.h" 
#include <stddef.h>

// =============================================================
//   Hardware & Low-Level I/O Emulation (extern "C")
// =============================================================
extern "C" {

    // ---------------------------------------------------------
    // File / Device Access
    // ---------------------------------------------------------

    // open: Open a file or device
    int my_open(const char* pathname, int flags, ...);

    // close: Close a file or device
    int my_close(int fd);

    // read: Reading
    int my_read(int fd, void* buf, size_t count);

    // write: Writing
    int my_write(int fd, const void* buf, size_t count);

    // ---------------------------------------------------------
    // Device Control & Advanced I/O
    // ---------------------------------------------------------

    // ioctl: I/O Control
    int my_ioctl(int fd, unsigned long request, ...);

    // writev: Vector Write
    int my_writev(int fd, const struct iovec* iov, int iovcnt);
}
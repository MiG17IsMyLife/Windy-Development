#define _CRT_SECURE_NO_WARNINGS
#include "HardwareBridge.h"

#include "../../hardware/BaseBoard.h"
#include "../../hardware/JvsBoard.h"
#include "../../hardware/EepromBoard.h"

#include <windows.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>

// =============================================================
//   Device Management Globals
// =============================================================

static std::map<int, LindberghDevice*> g_openDevices;
static int g_nextFd = 1000;

static BaseBoard g_baseBoard;
static EepromBoard g_eepromBoard;
static JvsBoard g_jvsBoard;

// =============================================================
//   Implementation (extern "C")
// =============================================================
extern "C" {

    // ---------------------------------------------------------
    // my_open
    // ---------------------------------------------------------
    int my_open(const char* pathname, int flags, ...) {
        std::cout << "[IO] open: " << pathname << std::endl;

        if (strncmp(pathname, "/proc/", 6) == 0) return -1;

        LindberghDevice* target = nullptr;

        // 【修正】 タイポを修正 (/dev/lbbb -> /dev/lbb)
        if (strstr(pathname, "/dev/lbb")) target = &g_baseBoard;
        else if (strstr(pathname, "/dev/i2c")) target = &g_eepromBoard;
        else if (strstr(pathname, "/dev/ttyS")) target = &g_jvsBoard;

        else if (strncmp(pathname, "/dev/", 5) == 0) {
            // 知らないデバイスはNULを開いておく（読み書きしても落ちないように）
            std::cout << "[IO] Unknown device, opening NUL: " << pathname << std::endl;
            return _open("NUL", _O_RDWR);
        }

        if (target) {
            std::cout << "[IO] Device found: " << pathname << " -> Simulated" << std::endl;
            if (target->Open()) {
                int fd = g_nextFd++;
                g_openDevices[fd] = target;
                return fd;
            }
            else {
                std::cerr << "[IO] Failed to open device: " << pathname << std::endl;
                return -1;
            }
        }

        int win_flags = _O_BINARY;

        if ((flags & 3) == 0) win_flags |= _O_RDONLY;      // O_RDONLY
        if ((flags & 3) == 1) win_flags |= _O_WRONLY;      // O_WRONLY
        if ((flags & 3) == 2) win_flags |= _O_RDWR;        // O_RDWR
        if (flags & 0x40) win_flags |= _O_CREAT;           // O_CREAT
        if (flags & 0x400) win_flags |= _O_TRUNC;          // O_TRUNC

        int mode = _S_IREAD | _S_IWRITE;
        if (flags & 0x40) {
            va_list args;
            va_start(args, flags);
            // mode = va_arg(args, int); // 必要なら取得
            va_end(args);
        }

        return _open(pathname, win_flags, mode);
    }

    // ---------------------------------------------------------
    // my_close
    // ---------------------------------------------------------
    int my_close(int fd) {
        if (g_openDevices.count(fd)) {
            g_openDevices[fd]->Close();
            g_openDevices.erase(fd);
            return 0;
        }
        return _close(fd);
    }

    // ---------------------------------------------------------
    // my_read
    // ---------------------------------------------------------
    int my_read(int fd, void* buf, size_t count) {
        // 仮想デバイスへの読み込み
        if (g_openDevices.count(fd)) {
            return g_openDevices[fd]->Read(buf, count);
        }
        // 通常ファイルへの読み込み
        return _read(fd, buf, (unsigned int)count);
    }

    // ---------------------------------------------------------
    // my_write
    // ---------------------------------------------------------
    int my_write(int fd, const void* buf, size_t count) {
        if (g_openDevices.count(fd)) {
            return g_openDevices[fd]->Write(buf, count);
        }

        if (fd == 1 || fd == 2) {
            fwrite(buf, 1, count, stdout);
            fflush(stdout);
            return count;
        }

        return _write(fd, buf, (unsigned int)count);
    }

    // ---------------------------------------------------------
    // my_ioctl
    // ---------------------------------------------------------
    int my_ioctl(int fd, unsigned long request, ...) {
        va_list args;
        va_start(args, request);
        void* argp = va_arg(args, void*);
        va_end(args);

        if (g_openDevices.count(fd)) {
            return g_openDevices[fd]->Ioctl(request, argp);
        }

        return 0;
    }

    // ---------------------------------------------------------
    // my_writev (Vector Write implementation)
    // ---------------------------------------------------------
    int my_writev(int fd, const struct iovec* iov, int iovcnt) {
        int total_written = 0;

        for (int i = 0; i < iovcnt; ++i) {
            const void* base = iov[i].iov_base;
            size_t len = iov[i].iov_len;

            if (len > 0) {
                int res = my_write(fd, base, len);

                if (res < 0) {
                    if (total_written > 0) return total_written;
                    return -1;
                }
                total_written += res;
            }
        }
        return total_written;
    }
}
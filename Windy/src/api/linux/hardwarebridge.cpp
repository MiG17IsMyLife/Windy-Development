#define _CRT_SECURE_NO_WARNINGS

#include "HardwareBridge.h"
#include "LinuxTypes.h"
#include "../src/core/log.h"

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>

// Hardware emulation includes
#include "../../hardware/BaseBoard.h"
#include "../../hardware/EepromBoard.h"
#include "../../hardware/JvsBoard.h"

// =============================================================
//   Virtual File Descriptor Management
// =============================================================

// Device types
enum DeviceType {
    DEV_NONE = 0,
    DEV_BASEBOARD,
    DEV_EEPROM,
    DEV_JVS,
    DEV_SERIAL,
    DEV_REAL_FILE
};

struct VirtualFD {
    DeviceType type;
    LindberghDevice* device;
    int realFd;  // For real files
    std::string path;
};

static std::map<int, VirtualFD> g_fdMap;
static int g_nextVirtualFd = 1000;  // Start virtual FDs at 1000 to avoid conflicts

// Device instances
static BaseBoard g_baseBoard;
static EepromBoard g_eepromBoard;
static JvsBoard g_jvsBoard;
static bool g_devicesInitialized = false;

static void EnsureDevicesInitialized() {
    if (!g_devicesInitialized) {
        g_baseBoard.Open();
        g_eepromBoard.Open();
        g_jvsBoard.Open();
        g_devicesInitialized = true;
        log_debug("Hardware devices initialized");
    }
}

// =============================================================
//   Path to Device Mapping
// =============================================================

static DeviceType GetDeviceType(const char* path) {
    if (!path) return DEV_NONE;
    if (strstr(path, "/dev/lbb") || strstr(path, "lbb")) return DEV_BASEBOARD;
    if (strstr(path, "/dev/i2c") || strstr(path, "i2c/")) return DEV_EEPROM;
    if (strstr(path, "/dev/ttyS") || strstr(path, "ttyS")) return DEV_SERIAL;
    if (strstr(path, "/dev/jvs") || strstr(path, "jvs")) return DEV_JVS;

    return DEV_REAL_FILE;
}

static LindberghDevice* GetDeviceInstance(DeviceType type) {
    switch (type) {
    case DEV_BASEBOARD: return &g_baseBoard;
    case DEV_EEPROM: return &g_eepromBoard;
    case DEV_JVS: return &g_jvsBoard;
    default: return nullptr;
    }
}

// =============================================================
//   Linux File Flag Conversion
// =============================================================

static int ConvertLinuxFlags(int linuxFlags) {
    int winFlags = 0;

    // Access mode
    int accessMode = linuxFlags & 3;  // O_RDONLY=0, O_WRONLY=1, O_RDWR=2
    if (accessMode == 0) winFlags |= _O_RDONLY;
    else if (accessMode == 1) winFlags |= _O_WRONLY;
    else if (accessMode == 2) winFlags |= _O_RDWR;

    // Other flags
    if (linuxFlags & 0x40) winFlags |= _O_CREAT;    // O_CREAT
    if (linuxFlags & 0x200) winFlags |= _O_TRUNC;   // O_TRUNC
    if (linuxFlags & 0x400) winFlags |= _O_APPEND;  // O_APPEND

    winFlags |= _O_BINARY;  // Always binary mode

    return winFlags;
}

// =============================================================
//   Implementation (extern "C")
// =============================================================

extern "C" {

    int my_open(const char* pathname, int flags, ...) {
        EnsureDevicesInitialized();

        DeviceType devType = GetDeviceType(pathname);

        log_debug("open(\"%s\", 0x%X) -> device type %d", pathname, flags, devType);

        if (devType != DEV_REAL_FILE && devType != DEV_NONE) {
            // Virtual device
            int vfd = g_nextVirtualFd++;
            VirtualFD vf;
            vf.type = devType;
            vf.device = GetDeviceInstance(devType);
            vf.realFd = -1;
            vf.path = pathname;
            g_fdMap[vfd] = vf;

            log_debug("open: Virtual FD %d for device %s", vfd, pathname);
            return vfd;
        }

        // Real file
        int winFlags = ConvertLinuxFlags(flags);
        int fd = _open(pathname, winFlags, 0666);

        if (fd >= 0) {
            VirtualFD vf;
            vf.type = DEV_REAL_FILE;
            vf.device = nullptr;
            vf.realFd = fd;
            vf.path = pathname;
            g_fdMap[fd] = vf;
            log_trace("open: Real FD %d for file %s", fd, pathname);
        }
        else {
            log_trace("open: Failed to open %s", pathname);
        }

        return fd;
    }

    int my_close(int fd) {
        auto it = g_fdMap.find(fd);
        if (it == g_fdMap.end()) {
            return _close(fd);
        }

        VirtualFD& vf = it->second;

        if (vf.type == DEV_REAL_FILE) {
            int result = _close(vf.realFd);
            g_fdMap.erase(it);
            return result;
        }

        // Virtual device - just remove from map
        log_trace("close: Virtual FD %d (%s)", fd, vf.path.c_str());
        g_fdMap.erase(it);
        return 0;
    }

    int my_read(int fd, void* buf, size_t count) {
        auto it = g_fdMap.find(fd);
        if (it == g_fdMap.end()) {
            return _read(fd, buf, (unsigned int)count);
        }

        VirtualFD& vf = it->second;

        if (vf.type == DEV_REAL_FILE) {
            return _read(vf.realFd, buf, (unsigned int)count);
        }

        // Virtual device
        if (vf.device) {
            return vf.device->Read(buf, count);
        }

        return -1;
    }

    int my_write(int fd, const void* buf, size_t count) {
        auto it = g_fdMap.find(fd);
        if (it == g_fdMap.end()) {
            return _write(fd, buf, (unsigned int)count);
        }

        VirtualFD& vf = it->second;

        if (vf.type == DEV_REAL_FILE) {
            return _write(vf.realFd, buf, (unsigned int)count);
        }

        // Virtual device
        if (vf.device) {
            return vf.device->Write(buf, count);
        }

        return -1;
    }

    int my_ioctl(int fd, unsigned long request, ...) {
        va_list args;
        va_start(args, request);
        void* data = va_arg(args, void*);
        va_end(args);

        auto it = g_fdMap.find(fd);
        if (it == g_fdMap.end()) {
            log_warn("ioctl: Unknown FD %d, request 0x%lX", fd, request);
            return -1;
        }

        VirtualFD& vf = it->second;

        if (vf.type == DEV_REAL_FILE) {
            log_warn("ioctl: Cannot ioctl real file FD %d", fd);
            return -1;
        }

        // Virtual device
        if (vf.device) {
            return vf.device->Ioctl(request, data);
        }

        log_warn("ioctl: No device for FD %d", fd);
        return -1;
    }

    int my_writev(int fd, const struct iovec* iov, int iovcnt) {
        int totalWritten = 0;

        for (int i = 0; i < iovcnt; i++) {
            int written = my_write(fd, iov[i].iov_base, iov[i].iov_len);
            if (written < 0) return written;
            totalWritten += written;
        }

        return totalWritten;
    }

} // extern "C"
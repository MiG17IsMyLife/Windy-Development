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
#include <algorithm>
#include <vector>

// Hardware emulation includes
#include "../../hardware/BaseBoard.h"
#include "../../hardware/EepromBoard.h"
#include "../../hardware/JvsBoard.h"
#include "../../hardware/SerialBoard.h"

// =============================================================
//   Unknown Device (Fallback for unhandled /dev/ nodes)
// =============================================================
class UnknownDevice : public LindberghDevice {
public:
    UnknownDevice() {}
    virtual ~UnknownDevice() {}

    bool Open() override { return true; }
    void Close() override {}

    int Read(void* buf, size_t count) override {
        // Return 0 (EOF) to prevent blocking, but log just in case
        // log_warn("UnknownDevice: Read of %zu bytes", count);
        return 0;
    }

    int Write(const void* buf, size_t count) override {
        // log_warn("UnknownDevice: Write of %zu bytes", count);
        return (int)count;
    }

    int Ioctl(unsigned long request, void* data) override {
        log_warn("UnknownDevice: Ioctl request 0x%lX", request);
        return -1;
    }
};

// =============================================================
//   Virtual File Descriptor Management
// =============================================================

enum DeviceType {
    DEV_NONE = 0,
    DEV_BASEBOARD,      // /dev/lbb
    DEV_EEPROM,         // /dev/i2c/0
    DEV_JVS,            // /dev/jvs
    DEV_SERIAL,         // /dev/ttyS0
    DEV_UNKNOWN,        // Any other /dev/ path
    DEV_REAL_FILE       // Real file on Windows filesystem
};

struct FileDescriptorEntry {
    DeviceType type;
    LindberghDevice* device;
    int hostFd;
    std::string path;
    int flags;
};

static std::map<int, FileDescriptorEntry> g_fdTable;

// Device singleton instances
static BaseBoard g_baseBoard;
static EepromBoard g_eepromBoard;
static JvsBoard g_jvsBoard;
static SerialBoard g_serialBoard;
static UnknownDevice g_unknownDevice;
static bool g_devicesInitialized = false;

// Helper to stringify device types for logging
static const char* GetDeviceName(DeviceType type) {
    switch (type) {
    case DEV_BASEBOARD: return "BaseBoard";
    case DEV_EEPROM:    return "EEPROM";
    case DEV_JVS:       return "JVS";
    case DEV_SERIAL:    return "Serial";
    case DEV_REAL_FILE: return "File";
    default:            return "Unknown";
    }
}

static void EnsureDevicesInitialized() {
    if (!g_devicesInitialized) {
        log_info("HardwareBridge: Initializing hardware devices...");

        // Link BaseBoard to JvsBoard
        g_baseBoard.SetJvsBoard(&g_jvsBoard);

        if (g_baseBoard.Open()) log_info(" - BaseBoard initialized");
        if (g_eepromBoard.Open()) log_info(" - EepromBoard initialized");
        if (g_jvsBoard.Open()) log_info(" - JvsBoard initialized");
        if (g_serialBoard.Open()) log_info(" - SerialBoard initialized");

        g_devicesInitialized = true;
    }
}

static int AllocateGuestFd() {
    for (int i = 3; i < 1024; ++i) {
        if (g_fdTable.find(i) == g_fdTable.end()) {
            return i;
        }
    }
    log_error("HardwareBridge: FD table exhausted!");
    return -1;
}

static DeviceType IdentifyDevice(const char* pathname) {
    if (!pathname) return DEV_NONE;

    std::string path = pathname;
    std::replace(path.begin(), path.end(), '\\', '/');

    if (path.find("/dev/lbb") != std::string::npos || path == "lbb") return DEV_BASEBOARD;
    if (path.find("i2c") != std::string::npos) return DEV_EEPROM;
    if (path.find("jvs") != std::string::npos) return DEV_JVS;
    if (path.find("ttyS") != std::string::npos) return DEV_SERIAL;

    // Catch-all for other /dev/ paths
    if (path.find("/dev/") == 0) {
        log_warn("IdentifyDevice: Unhandled device path '%s' -> Mapping to UNKNOWN", pathname);
        return DEV_UNKNOWN;
    }

    return DEV_REAL_FILE;
}

static LindberghDevice* GetDeviceInstance(DeviceType type) {
    switch (type) {
    case DEV_BASEBOARD: return &g_baseBoard;
    case DEV_EEPROM:    return &g_eepromBoard;
    case DEV_JVS:       return &g_jvsBoard;
    case DEV_SERIAL:    return &g_serialBoard;
    case DEV_UNKNOWN:   return &g_unknownDevice;
    default:            return nullptr;
    }
}

static int ConvertFlags(int linuxFlags) {
    int winFlags = _O_BINARY;
    if ((linuxFlags & 3) == 0) winFlags |= _O_RDONLY;
    else if ((linuxFlags & 3) == 1) winFlags |= _O_WRONLY;
    else if ((linuxFlags & 3) == 2) winFlags |= _O_RDWR;
    if (linuxFlags & 0x40) winFlags |= _O_CREAT;
    if (linuxFlags & 0x200) winFlags |= _O_TRUNC;
    if (linuxFlags & 0x400) winFlags |= _O_APPEND;
    return winFlags;
}

// =============================================================
//   Extern C Interface (Hooked Functions)
// =============================================================

extern "C" {

    int my_open(const char* pathname, int flags, ...) {
        EnsureDevicesInitialized();

        if (!pathname) return -1;

        DeviceType type = IdentifyDevice(pathname);
        int guestFd = AllocateGuestFd();
        if (guestFd < 0) return -1;

        FileDescriptorEntry entry;
        entry.type = type;
        entry.path = pathname;
        entry.flags = flags;
        entry.device = nullptr;
        entry.hostFd = -1;

        if (type == DEV_REAL_FILE) {
            int winFlags = ConvertFlags(flags);
            int hostFd = _open(pathname, winFlags, 0666);

            if (hostFd < 0) {
                if (strncmp(pathname, "/dev/", 5) == 0) {
                    log_warn("open: Real file failed for '%s', forcing UNKNOWN device.", pathname);
                    entry.type = DEV_UNKNOWN;
                    entry.device = &g_unknownDevice;
                    g_fdTable[guestFd] = entry;
                    return guestFd;
                }
                log_debug("open: Failed to open real file '%s' (errno=%d)", pathname, errno);
                return -1;
            }
            entry.hostFd = hostFd;
            log_debug("open[%d]: Real file '%s' (HostFD: %d)", guestFd, pathname, hostFd);
        }
        else {
            entry.device = GetDeviceInstance(type);
            if (!entry.device) return -1;
            log_info("open[%d]: Virtual Device '%s' (%s)", guestFd, pathname, GetDeviceName(type));
        }

        g_fdTable[guestFd] = entry;
        return guestFd;
    }

    int my_close(int fd) {
        auto it = g_fdTable.find(fd);
        if (it == g_fdTable.end()) {
            if (fd >= 0 && fd <= 2) return 0;
            return -1;
        }

        FileDescriptorEntry& entry = it->second;

        if (entry.type == DEV_REAL_FILE) {
            _close(entry.hostFd);
            log_debug("close[%d]: Real file '%s'", fd, entry.path.c_str());
        }
        else {
            if (entry.device) entry.device->Close();
            log_info("close[%d]: Virtual Device '%s'", fd, entry.path.c_str());
        }

        g_fdTable.erase(it);
        return 0;
    }

    int my_read(int fd, void* buf, size_t count) {
        auto it = g_fdTable.find(fd);
        if (it == g_fdTable.end()) {
            return _read(fd, buf, (unsigned int)count);
        }

        FileDescriptorEntry& entry = it->second;
        int result = -1;

        if (entry.type == DEV_REAL_FILE) {
            result = _read(entry.hostFd, buf, (unsigned int)count);
        }
        else if (entry.device) {
            result = entry.device->Read(buf, count);

            if (result > 0 && entry.type != DEV_EEPROM) {
                log_debug("read[%d] (%s): %d bytes", fd, GetDeviceName(entry.type), result);
            }
        }

        return result;
    }

    int my_write(int fd, const void* buf, size_t count) {
        auto it = g_fdTable.find(fd);
        if (it == g_fdTable.end()) {
            return _write(fd, buf, (unsigned int)count);
        }

        FileDescriptorEntry& entry = it->second;
        int result = -1;

        if (entry.type == DEV_REAL_FILE) {
            result = _write(entry.hostFd, buf, (unsigned int)count);
        }
        else if (entry.device) {
            if (entry.type != DEV_EEPROM) {
                log_debug("write[%d] (%s): %d bytes", fd, GetDeviceName(entry.type), count);
            }
            result = entry.device->Write(buf, count);
        }

        return result;
    }

    int my_ioctl(int fd, unsigned long request, ...) {
        va_list args;
        va_start(args, request);
        void* data = va_arg(args, void*);
        va_end(args);

        // ==========================================================
        // FD -1 Handling (Game Bug Workaround)
        // ==========================================================
        if (fd == -1) {
            unsigned long reqCmd = request & 0xFFFF;
            // 0x720=SMBUS, 0x703=SLAVE, 0x705=FUNCS, 0x1261=CLEAR
            if (reqCmd == 0x0720 || reqCmd == 0x0703 || reqCmd == 0x0705 || reqCmd == 0x1261) {
                EnsureDevicesInitialized();
                log_debug("ioctl[-1]: Routing I2C request 0x%lX to EEPROM (Game Bug Workaround)", request);
                return g_eepromBoard.Ioctl(request, data);
            }
            log_error("ioctl[-1]: Unhandled request 0x%lX", request);
            return -1;
        }

        auto it = g_fdTable.find(fd);
        if (it == g_fdTable.end()) {
            log_error("ioctl[%d]: Invalid FD! (Req: 0x%lX)", fd, request);
            return -1;
        }

        FileDescriptorEntry& entry = it->second;

        if (entry.type == DEV_REAL_FILE) {
            return -1;
        }

        if (entry.device) {
            log_debug("ioctl[%d] (%s): Req 0x%lX", fd, GetDeviceName(entry.type), request);

            int ret = entry.device->Ioctl(request, data);

            if (ret != 0) {
                log_warn("ioctl[%d] (%s): Req 0x%lX failed with %d", fd, GetDeviceName(entry.type), request, ret);
            }
            return ret;
        }

        return -1;
    }

    int my_writev(int fd, const struct iovec* iov, int iovcnt) {
        int totalWritten = 0;
        for (int i = 0; i < iovcnt; i++) {
            int res = my_write(fd, iov[i].iov_base, iov[i].iov_len);
            if (res < 0) return (totalWritten > 0) ? totalWritten : -1;
            totalWritten += res;
        }
        return totalWritten;
    }

} // extern "C"
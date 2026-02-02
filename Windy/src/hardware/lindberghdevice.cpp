#define _CRT_SECURE_NO_WARNINGS

#include "lindberghdevice.h"
#include "../core/log.h"
#include "../core/config.h"
#include <cstring>
#include <cstdio>

// Device path constants
static const char* DEV_BASEBOARD = "/dev/lbb";
static const char* DEV_EEPROM_DASH = "/dev/i2c-";    // /dev/i2c-0, /dev/i2c-1, etc.
static const char* DEV_EEPROM_SLASH = "/dev/i2c/";   // /dev/i2c/0, /dev/i2c/1, etc.
static const char* DEV_SERIAL = "/dev/ttyS";
static const char* DEV_PCI_1F0 = "/proc/bus/pci/00/1f.0";
static const char* DEV_PCI_000 = "/proc/bus/pci/00/00.0";
static const char* DEV_PROC_NET_ROUTE = "/proc/net/route";
static const char* DEV_DHCP_PID = "/var/run/dhcpcd-eth0.pid";

// Starting fake FD number (avoid conflicts with real FDs)
static const int FAKE_FD_START = 1000;

// ============================================================
// Singleton Instance
// ============================================================

LindberghDevice& LindberghDevice::Instance() {
    static LindberghDevice instance;
    return instance;
}

// ============================================================
// Constructor / Destructor
// ============================================================

LindberghDevice::LindberghDevice()
    : m_nextFd(FAKE_FD_START)
{
}

LindberghDevice::~LindberghDevice() {
    Cleanup();
}

// ============================================================
// Init - Initialize all hardware components
// ============================================================

bool LindberghDevice::Init() {
    log_info("LindberghDevice: Initializing...");

    // Initialize JvsBoard
    if (!m_jvsBoard.Open()) {
        log_error("LindberghDevice: Failed to initialize JvsBoard");
        return false;
    }

    // Initialize EepromBoard
    if (!m_eepromBoard.Open()) {
        log_error("LindberghDevice: Failed to initialize EepromBoard");
        return false;
    }

    // Initialize BaseBoard (connects to JvsBoard)
    m_baseBoard.SetJvsBoard(&m_jvsBoard);
    if (!m_baseBoard.Open()) {
        log_error("LindberghDevice: Failed to initialize BaseBoard");
        return false;
    }

    // Initialize SerialBoard
    if (!m_serialBoard.Open()) {
        log_warn("LindberghDevice: SerialBoard initialization skipped (optional)");
    }

    log_info("LindberghDevice: Initialization complete");
    return true;
}

// ============================================================
// Cleanup
// ============================================================

void LindberghDevice::Cleanup() {
    log_info("LindberghDevice: Shutting down...");

    m_baseBoard.Close();
    m_jvsBoard.Close();
    m_eepromBoard.Close();
    m_serialBoard.Close();

    m_fileDescriptors.clear();

    log_info("LindberghDevice: Shutdown complete");
}

// ============================================================
// File Descriptor Emulation - Open
// ============================================================

int LindberghDevice::Open(const char* path, int flags) {
    if (!path) return -1;

    EmulatedDeviceType type = DEVICE_NONE;

    // Match device path
    if (strncmp(path, DEV_BASEBOARD, strlen(DEV_BASEBOARD)) == 0) {
        type = DEVICE_BASEBOARD;
        log_debug("LindberghDevice::Open: BaseBoard (%s)", path);
    }
    else if (strncmp(path, DEV_EEPROM_DASH, strlen(DEV_EEPROM_DASH)) == 0 ||
        strncmp(path, DEV_EEPROM_SLASH, strlen(DEV_EEPROM_SLASH)) == 0) {
        type = DEVICE_EEPROM;
        log_debug("LindberghDevice::Open: EEPROM/I2C (%s)", path);
    }
    else if (strncmp(path, DEV_SERIAL, strlen(DEV_SERIAL)) == 0) {
        type = DEVICE_SERIAL;
        log_debug("LindberghDevice::Open: Serial (%s)", path);
    }
    else if (strcmp(path, DEV_PCI_1F0) == 0) {
        type = DEVICE_PCI_1F0;
        log_debug("LindberghDevice::Open: PCI 1F0 (%s)", path);
    }
    else if (strcmp(path, DEV_PCI_000) == 0) {
        type = DEVICE_PCI_000;
        log_debug("LindberghDevice::Open: PCI 000 (%s)", path);
    }
    else if (strcmp(path, DEV_PROC_NET_ROUTE) == 0) {
        type = DEVICE_PROC_NET_ROUTE;
        log_debug("LindberghDevice::Open: Emulating /proc/net/route");
    }
    else if (strstr(path, "dhcpcd-eth0.pid") != NULL) {
        type = DEVICE_DHCP_PID;
        log_debug("LindberghDevice::Open: Emulating DHCP PID (%s)", path);
    }
    else {
        if (strncmp(path, "/dev/", 5) == 0 || strncmp(path, "/proc/", 6) == 0) {
            log_warn("LindberghDevice::Open: Unknown device path: %s", path);
        }
        else {
            log_debug("LindberghDevice::Open: Passthrough to OS fs: %s", path);
        }
        return -1;
    }

    // Assign fake file descriptor
    int fd = m_nextFd++;
    m_fileDescriptors[fd] = { type, flags, 0 };

    return fd;
}

// ============================================================
// File Descriptor Emulation - Close
// ============================================================

int LindberghDevice::Close(int fd) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) {
        return -1;
    }

    m_fileDescriptors.erase(it);
    return 0;
}

// ============================================================
// File Descriptor Emulation - Read
// ============================================================

int LindberghDevice::Read(int fd, void* buf, size_t count) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) {
        return -1;
    }

    FdInfo& info = it->second;

    switch (info.type) {
    case DEVICE_BASEBOARD:
        return m_baseBoard.Read((char*)buf, count);

    case DEVICE_PCI_1F0:
    case DEVICE_PCI_000:
    {
        // PCI config space read emulation
        // Return dummy data for Intel ICH9 (0x29408086)
        if (count >= 4) {
            uint32_t* data = (uint32_t*)buf;
            if (info.type == DEVICE_PCI_1F0) {
                // ICH9 LPC Interface
                *data = 0x29448086;
            }
            else {
                // Intel MCH
                *data = 0x29408086;
            }
            return 4;
        }
        return 0;
    }

    case DEVICE_SERIAL:
        return m_serialBoard.Read((char*)buf, (unsigned int)count);

    case DEVICE_PROC_NET_ROUTE:
    {
        const char* routeData =
            "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n"
            "eth0\t00000000\t00000000\t0001\t0\t0\t0\t00000000\t0\t0\t0\n";

        size_t len = strlen(routeData);
        if (info.offset >= len) return 0;

        size_t toCopy = len - info.offset;
        if (toCopy > count) toCopy = count;

        memcpy(buf, routeData + info.offset, toCopy);
        info.offset += toCopy;
        return (int)toCopy;
    }

    case DEVICE_DHCP_PID:
    {
        const char* pidData = "1000\n"; //Fake number
        size_t len = strlen(pidData);

        if (info.offset >= len) return 0;

        size_t toCopy = len - info.offset;
        if (toCopy > count) toCopy = count;

        memcpy(buf, pidData + info.offset, toCopy);
        info.offset += toCopy;
        return (int)toCopy;
    }

    default:
        return -1;
    }
}

// ============================================================
// File Descriptor Emulation - Write
// ============================================================

int LindberghDevice::Write(int fd, const void* buf, size_t count) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) {
        return -1;
    }

    FdInfo& info = it->second;

    switch (info.type) {
    case DEVICE_BASEBOARD:
        return m_baseBoard.Write((const char*)buf, count);

    case DEVICE_SERIAL:
        return m_serialBoard.Write((const char*)buf, (unsigned int)count);

    case DEVICE_PCI_1F0:
    case DEVICE_PCI_000:
        // PCI config writes are ignored
        return (int)count;

    default:
        return -1;
    }
}

// ============================================================
// File Descriptor Emulation - Ioctl
// ============================================================

int LindberghDevice::Ioctl(int fd, unsigned long request, void* data) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) {
        return -1;
    }

    FdInfo& info = it->second;

    switch (info.type) {
    case DEVICE_BASEBOARD:
        return m_baseBoard.Ioctl((unsigned int)request, data);

    case DEVICE_EEPROM:
        return m_eepromBoard.Ioctl((unsigned int)request, data);

    case DEVICE_SERIAL:
        return m_serialBoard.Ioctl((unsigned int)request, data);

    default:
        log_warn("LindberghDevice::Ioctl: Unhandled device type for fd %d", fd);
        return -1;
    }
}

// ============================================================
// File Descriptor Emulation - Seek
// ============================================================

int LindberghDevice::Seek(int fd, long offset, int whence) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) {
        return -1;
    }

    FdInfo& info = it->second;

    switch (whence) {
    case SEEK_SET:
        info.offset = offset;
        break;
    case SEEK_CUR:
        info.offset += offset;
        break;
    case SEEK_END:
        // For PCI config space, end is typically 256 bytes
        info.offset = 256 + offset;
        break;
    default:
        return -1;
    }

    return 0;
}

// ============================================================
// File Descriptor Emulation - Tell
// ============================================================

long LindberghDevice::Tell(int fd) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) {
        return -1;
    }

    return (long)it->second.offset;
}

// ============================================================
// Port I/O Emulation - Read
// ============================================================

int LindberghDevice::PortRead(uint16_t port, uint32_t* data) {
    // Security Board / DIP switch port I/O
    return m_securityBoard.PortRead(port, data);
}

// ============================================================
// Port I/O Emulation - Write
// ============================================================

int LindberghDevice::PortWrite(uint16_t port, uint32_t data) {
    // Security Board / DIP switch port I/O
    return m_securityBoard.PortWrite(port, data);
}
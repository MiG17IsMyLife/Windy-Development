#include "lindberghdevice.h"
#include "../core/log.h"
#include <cstring>
#include <stdio.h>

#include "pciData.h"

// ========================================================================
// --- PCI Configuration Space Ports ---
// ========================================================================
#define PCI_CONFIG_ADDRESS  0x0CF8
#define PCI_CONFIG_DATA     0x0CFC

// Static PCI address register (shared state)
static uint32_t g_pciConfigAddress = 0;

LindberghDevice& LindberghDevice::Instance() {
    static LindberghDevice instance;
    return instance;
}

LindberghDevice::LindberghDevice() : m_nextFd(100) {
}

LindberghDevice::~LindberghDevice() {
    Cleanup();
}

bool LindberghDevice::Init() {
    log_info("LindberghDevice: Initializing Hardware...");

    // 1. JVS Board
    if (!m_jvsBoard.Open()) {
        log_error("LindberghDevice: Failed to open JVS Board");
        return false;
    }

    // 2. Base Board
    m_baseBoard.SetJvsBoard(&m_jvsBoard);
    if (!m_baseBoard.Open()) {
        log_error("LindberghDevice: Failed to open Base Board");
        return false;
    }

    // 3. EEPROM
    if (!m_eepromBoard.Open("eeprom.bin")) {
        log_error("LindberghDevice: Failed to open EEPROM Board");
        return false;
    }

    // 4. Serial
    if (!m_serialBoard.Open()) {
        log_warn("LindberghDevice: Failed to open Serial Board");
    }

    // 5. Security Board (DIP switches for resolution)
    m_securityBoard.SetDipResolution(1360, 768);

    log_info("LindberghDevice: Hardware Initialized.");
    return true;
}

void LindberghDevice::Cleanup() {
    m_baseBoard.Close();
    m_jvsBoard.Close();
    m_eepromBoard.Close();
    m_serialBoard.Close();
    m_fileDescriptors.clear();
}

int LindberghDevice::Open(const char* path, int flags) {
    EmulatedDeviceType type = DEVICE_NONE;

    if (strstr(path, "/dev/lbb")) {
        type = DEVICE_BASEBOARD;
    }
    else if (strstr(path, "/dev/i2c") || strstr(path, "eeprom.bin")) {
        type = DEVICE_EEPROM;
    }
    else if (strstr(path, "/dev/ttyS") || strstr(path, "COM")) {
        type = DEVICE_SERIAL;
    }
    else if (strstr(path, "/proc/bus/pci/00/1f.0")) {
        type = DEVICE_PCI_1F0;
        log_info("LindberghDevice: Opening Virtual PCI 00:1f.0 (LPC Bridge)");
    }
    else if (strstr(path, "/proc/bus/pci/00/00.0")) {
        type = DEVICE_PCI_000;
        log_info("LindberghDevice: Opening Virtual PCI 00:00.0 (Host Bridge)");
    }

    if (type != DEVICE_NONE) {
        int fd = m_nextFd++;
        m_fileDescriptors[fd] = { type, flags, 0 };
        return fd;
    }
    return -1;
}

int LindberghDevice::Close(int fd) {
    auto it = m_fileDescriptors.find(fd);
    if (it != m_fileDescriptors.end()) {
        m_fileDescriptors.erase(it);
        return 0;
    }
    return -1;
}

int LindberghDevice::Read(int fd, void* buf, size_t count) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) return -1;

    switch (it->second.type) {
    case DEVICE_PCI_1F0: {
        size_t avail = (256 > it->second.offset) ? (256 - it->second.offset) : 0;
        size_t n = (count < avail) ? count : avail;
        if (n > 0) {
            memcpy(buf, (const uint8_t*)pci_1f0 + it->second.offset, n);
            it->second.offset += n;
        }
        return (int)n;
    }
    case DEVICE_PCI_000: {
        size_t avail = (4096 > it->second.offset) ? (4096 - it->second.offset) : 0;
        size_t n = (count < avail) ? count : avail;
        if (n > 0) {
            memcpy(buf, (const uint8_t*)pci_000 + it->second.offset, n);
            it->second.offset += n;
        }
        return (int)n;
    }
    case DEVICE_BASEBOARD: return m_baseBoard.Read(buf, count);
    case DEVICE_SERIAL: return m_serialBoard.Read(buf, count);
    default: return 0;
    }
}

int LindberghDevice::Ioctl(int fd, unsigned long request, void* data) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) return -1;

    switch (it->second.type) {
    case DEVICE_BASEBOARD: return m_baseBoard.Ioctl(request, data);
    case DEVICE_EEPROM: return m_eepromBoard.Ioctl(request, data);
    case DEVICE_SERIAL: return m_serialBoard.Ioctl(request, data);
    default: return -1;
    }
}

int LindberghDevice::Seek(int fd, long offset, int whence) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) return -1;

    size_t size = 0;
    if (it->second.type == DEVICE_PCI_1F0) size = 256;
    else if (it->second.type == DEVICE_PCI_000) size = 4096;
    else return -1;

    size_t newPos = it->second.offset;
    if (whence == SEEK_SET) newPos = offset;
    else if (whence == SEEK_CUR) newPos += offset;
    else if (whence == SEEK_END) newPos = size + offset;

    if (newPos > size) newPos = size;
    it->second.offset = newPos;
    return 0;
}

long LindberghDevice::Tell(int fd) {
    auto it = m_fileDescriptors.find(fd);
    if (it != m_fileDescriptors.end()) return (long)it->second.offset;
    return -1;
}

// ========================================================================
// --- Port I/O Emulation (Called from HardwareBridge VEH) ---
// ========================================================================

int LindberghDevice::PortRead(uint16_t port, uint32_t* data) {
    *data = 0xFFFFFFFF; // Default: all bits high (typical for non-existent hardware)

    switch (port) {
        // ====================================================================
        // PCI Configuration Space Access
        // ====================================================================
    case PCI_CONFIG_ADDRESS:
        *data = g_pciConfigAddress;
        log_trace("PortRead: PCI_CONFIG_ADDRESS -> 0x%08X", *data);
        return 0;

    case PCI_CONFIG_DATA:
    case PCI_CONFIG_DATA + 1:
    case PCI_CONFIG_DATA + 2:
    case PCI_CONFIG_DATA + 3:
    {
        // Extract bus/device/function/register from the address
        int offset = port - PCI_CONFIG_DATA;
        int reg = (g_pciConfigAddress & 0xFC) + offset;
        int func = (g_pciConfigAddress >> 8) & 0x07;
        int dev = (g_pciConfigAddress >> 11) & 0x1F;
        int bus = (g_pciConfigAddress >> 16) & 0xFF;
        bool enabled = (g_pciConfigAddress & 0x80000000) != 0;

        *data = 0xFFFFFFFF;

        if (enabled) {
            if (bus == 0 && dev == 0x1F && func == 0) {
                // ICH (LPC Controller) - 00:1f.0
                if (reg < (int)sizeof(pci_1f0)) {
                    *data = 0;
                    memcpy(data, &pci_1f0[reg], 4);
                }
            }
            else if (bus == 0 && dev == 0 && func == 0) {
                // Host Bridge - 00:00.0
                if (reg < (int)sizeof(pci_000)) {
                    *data = 0;
                    memcpy(data, &pci_000[reg], 4);
                }
            }
            log_trace("PortRead: PCI Config [%02X:%02X.%X] reg=0x%02X -> 0x%08X",
                bus, dev, func, reg, *data);
        }
        return 0;
    }

    // ====================================================================
    // Security Board (DIP Switches / Front Panel)
    // ====================================================================
    case 0x38:
    case 0x1038:
        m_securityBoard.In(port, data);
        log_trace("PortRead: SecurityBoard port=0x%04X -> 0x%08X", port, *data);
        return 0;

        // ====================================================================
        // LPC / ICH Related Ports (Intel Chipset)
        // ====================================================================
    case 0x2C:  // LPC Decode Range Register / Super I/O Index
    case 0x2D:  // Super I/O Data
    case 0x2E:  // Super I/O Config Index
    case 0x2F:  // Super I/O Config Data
        *data = 0x00;
        log_trace("PortRead: LPC/SuperIO port=0x%04X -> 0x%08X", port, *data);
        return 0;

        // ====================================================================
        // DMA Controller Ports (often accessed during init)
        // ====================================================================
    case 0x00: case 0x01: case 0x02: case 0x03:
    case 0x04: case 0x05: case 0x06: case 0x07:
    case 0x08: case 0x09: case 0x0A: case 0x0B:
    case 0x0C: case 0x0D: case 0x0E: case 0x0F:
    case 0xC0: case 0xC2: case 0xC4: case 0xC6:
    case 0xC8: case 0xCA: case 0xCC: case 0xCE:
    case 0xD0: case 0xD2: case 0xD4: case 0xD6:
    case 0xD8: case 0xDA: case 0xDC: case 0xDE:
        *data = 0x00;
        log_trace("PortRead: DMA port=0x%04X -> 0x%08X", port, *data);
        return 0;

        // ====================================================================
        // PIC (Programmable Interrupt Controller)
        // ====================================================================
    case 0x20: case 0x21:  // Master PIC
    case 0xA0: case 0xA1:  // Slave PIC
        *data = 0x00;
        log_trace("PortRead: PIC port=0x%04X -> 0x%08X", port, *data);
        return 0;

        // ====================================================================
        // PIT (Programmable Interval Timer)
        // ====================================================================
    case 0x40: case 0x41: case 0x42: case 0x43:
        *data = 0x00;
        log_trace("PortRead: PIT port=0x%04X -> 0x%08X", port, *data);
        return 0;

        // ====================================================================
        // RTC (Real Time Clock) / CMOS
        // ====================================================================
    case 0x70: case 0x71:
    case 0x72: case 0x73:
        *data = 0x00;
        log_trace("PortRead: RTC/CMOS port=0x%04X -> 0x%08X", port, *data);
        return 0;

        // ====================================================================
        // PS/2 Controller
        // ====================================================================
    case 0x60: case 0x64:
        *data = 0x00;
        log_trace("PortRead: PS/2 port=0x%04X -> 0x%08X", port, *data);
        return 0;

        // ====================================================================
        // ISA Bridge / Other Legacy Ports
        // ====================================================================
    case 0x61:  // System Control Port B
        *data = 0x00;
        log_trace("PortRead: System Control B -> 0x%08X", *data);
        return 0;

    case 0x80:  // POST Diagnostic Port
        *data = 0x00;
        return 0;

        // ====================================================================
        // Default: Unknown Port
        // ====================================================================
    default:
        // Only log unknown ports at debug level to avoid spam
        log_debug("PortRead: Unknown port=0x%04X -> 0x%08X (default)", port, *data);
        return 0;
    }
}

int LindberghDevice::PortWrite(uint16_t port, uint32_t data) {
    switch (port) {
        // ====================================================================
        // PCI Configuration Space Access
        // ====================================================================
    case PCI_CONFIG_ADDRESS:
        g_pciConfigAddress = data;
        log_trace("PortWrite: PCI_CONFIG_ADDRESS <- 0x%08X", data);
        return 0;

    case PCI_CONFIG_DATA:
    case PCI_CONFIG_DATA + 1:
    case PCI_CONFIG_DATA + 2:
    case PCI_CONFIG_DATA + 3:
        // PCI config writes are typically ignored in emulation
        log_trace("PortWrite: PCI_CONFIG_DATA port=0x%04X <- 0x%08X (ignored)", port, data);
        return 0;

        // ====================================================================
        // Security Board
        // ====================================================================
    case 0x38:
    case 0x1038:
        m_securityBoard.Out(port, &data);
        log_trace("PortWrite: SecurityBoard port=0x%04X <- 0x%08X", port, data);
        return 0;

        // ====================================================================
        // LPC / Super I/O
        // ====================================================================
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
        log_trace("PortWrite: LPC/SuperIO port=0x%04X <- 0x%08X (ignored)", port, data);
        return 0;

        // ====================================================================
        // DMA Controller
        // ====================================================================
    case 0x00: case 0x01: case 0x02: case 0x03:
    case 0x04: case 0x05: case 0x06: case 0x07:
    case 0x08: case 0x09: case 0x0A: case 0x0B:
    case 0x0C: case 0x0D: case 0x0E: case 0x0F:
    case 0xC0: case 0xC2: case 0xC4: case 0xC6:
    case 0xC8: case 0xCA: case 0xCC: case 0xCE:
    case 0xD0: case 0xD2: case 0xD4: case 0xD6:
    case 0xD8: case 0xDA: case 0xDC: case 0xDE:
        log_trace("PortWrite: DMA port=0x%04X <- 0x%08X", port, data);
        return 0;

        // ====================================================================
        // PIC
        // ====================================================================
    case 0x20: case 0x21:
    case 0xA0: case 0xA1:
        log_trace("PortWrite: PIC port=0x%04X <- 0x%08X", port, data);
        return 0;

        // ====================================================================
        // PIT
        // ====================================================================
    case 0x40: case 0x41: case 0x42: case 0x43:
        log_trace("PortWrite: PIT port=0x%04X <- 0x%08X", port, data);
        return 0;

        // ====================================================================
        // RTC / CMOS
        // ====================================================================
    case 0x70: case 0x71:
    case 0x72: case 0x73:
        log_trace("PortWrite: RTC/CMOS port=0x%04X <- 0x%08X", port, data);
        return 0;

        // ====================================================================
        // PS/2 Controller
        // ====================================================================
    case 0x60: case 0x64:
        log_trace("PortWrite: PS/2 port=0x%04X <- 0x%08X", port, data);
        return 0;

        // ====================================================================
        // System Control / POST
        // ====================================================================
    case 0x61:
        log_trace("PortWrite: System Control B <- 0x%08X", data);
        return 0;

    case 0x80:  // POST code - useful for debugging
        log_trace("PortWrite: POST code <- 0x%02X", data & 0xFF);
        return 0;

        // ====================================================================
        // Default
        // ====================================================================
    default:
        log_debug("PortWrite: Unknown port=0x%04X <- 0x%08X", port, data);
        return 0;
    }
}

int LindberghDevice::Write(int fd, const void* buf, size_t count) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) return -1;
    if (it->second.type == DEVICE_BASEBOARD) return m_baseBoard.Write(buf, count);
    if (it->second.type == DEVICE_SERIAL) return m_serialBoard.Write(buf, count);
    return 0;
}
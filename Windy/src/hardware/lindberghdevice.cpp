#include "lindberghdevice.h"
#include "../core/log.h"
#include <cstring>
#include <stdio.h>

#include "pciData.h"

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
    // 診断用ログ
    log_debug("PortRead: Port 0x%04X", port);

    // 1. Security Board
    if (port == 0x38 || port == 0x1038) {
        return m_securityBoard.In(port, data);
    }

    // 2. クラッシュが発生していたポート 0x2C (Intel チップセット関連)
    if (port == 0x2C) {
        *data = 0x00000000; // 暫定的に 0 を返す
        return 0;
    }

    // 3. その他、よく使われるポートへのダミー応答
    // 不明なポートは 0xFFFFFFFF (未接続) を返してフリーズを防ぐ
    *data = 0xFFFFFFFF;
    return 0;
}

int LindberghDevice::PortWrite(uint16_t port, uint32_t data) {
    log_debug("PortWrite: Port 0x%04X, Data 0x%08X", port, data);

    // Security Board
    if (port == 0x38 || port == 0x1038) {
        return m_securityBoard.Out(port, &data);
    }

    return 0;
}

int LindberghDevice::Write(int fd, const void* buf, size_t count) {
    auto it = m_fileDescriptors.find(fd);
    if (it == m_fileDescriptors.end()) return -1;
    if (it->second.type == DEVICE_BASEBOARD) return m_baseBoard.Write(buf, count);
    if (it->second.type == DEVICE_SERIAL) return m_serialBoard.Write(buf, count);
    return 0;
}
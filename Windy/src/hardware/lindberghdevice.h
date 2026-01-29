#pragma once

#include <vector>
#include <string>
#include <map>
#include <stdint.h>

#include "baseboard.h"
#include "jvsboard.h"
#include "eepromboard.h"
#include "securityboard.h"
#include "serialboard.h"

enum EmulatedDeviceType {
    DEVICE_NONE,
    DEVICE_BASEBOARD,
    DEVICE_EEPROM,
    DEVICE_JVS,
    DEVICE_SERIAL,
    DEVICE_PCI_1F0,
    DEVICE_PCI_000
};

struct FileEntry {
    EmulatedDeviceType type;
    int flags;
    size_t offset;
};

class LindberghDevice {
public:
    static LindberghDevice& Instance();

    bool Init();
    void Cleanup();

    int Open(const char* path, int flags);
    int Close(int fd);
    int Ioctl(int fd, unsigned long request, void* data);
    int Read(int fd, void* buf, size_t count);
    int Write(int fd, const void* buf, size_t count);

    // 仮想ファイル操作用の追加メソッド
    int Seek(int fd, long offset, int whence);
    long Tell(int fd);

    int PortRead(uint16_t port, uint32_t* data);
    int PortWrite(uint16_t port, uint32_t data);

    BaseBoard* GetBaseBoard() { return &m_baseBoard; }
    JvsBoard* GetJvsBoard() { return &m_jvsBoard; }
    EepromBoard* GetEepromBoard() { return &m_eepromBoard; }
    SecurityBoard* GetSecurityBoard() { return &m_securityBoard; }
    SerialBoard* GetSerialBoard() { return &m_serialBoard; }

private:
    LindberghDevice();
    ~LindberghDevice();

    LindberghDevice(const LindberghDevice&) = delete;
    LindberghDevice& operator=(const LindberghDevice&) = delete;

    BaseBoard     m_baseBoard;
    JvsBoard      m_jvsBoard;
    EepromBoard   m_eepromBoard;
    SecurityBoard m_securityBoard;
    SerialBoard   m_serialBoard;

    std::map<int, FileEntry> m_fileDescriptors;
    int m_nextFd;
};
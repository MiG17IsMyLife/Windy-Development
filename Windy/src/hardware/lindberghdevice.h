#pragma once

#include <map>
#include <stdint.h>
#include <stddef.h>

#include "baseboard.h"
#include "jvsboard.h"
#include "eepromboard.h"
#include "securityboard.h"
#include "serialboard.h"

/**
 * @brief Emulated device types for file descriptor mapping
 */
enum EmulatedDeviceType {
    DEVICE_NONE = 0,
    DEVICE_BASEBOARD,
    DEVICE_EEPROM,
    DEVICE_SERIAL,
    DEVICE_PCI_1F0,
    DEVICE_PCI_000
};

/**
 * @brief File descriptor info for emulated devices
 */
struct FdInfo {
    EmulatedDeviceType type;
    int flags;
    size_t offset;  // For PCI config space reads
};

/**
 * @brief Lindbergh Device Manager (Singleton)
 *
 * Central manager for all Lindbergh hardware emulation.
 * Integrates BaseBoard, JvsBoard, EepromBoard, SecurityBoard, SerialBoard.
 * Provides file descriptor emulation for Linux device nodes.
 */
class LindberghDevice {
public:
    /**
     * @brief Get singleton instance
     */
    static LindberghDevice& Instance();

    /**
     * @brief Initialize all hardware components
     * @return true if successful
     */
    bool Init();

    /**
     * @brief Cleanup all hardware components
     */
    void Cleanup();

    // ============================================================
    // File Descriptor Emulation (for HardwareBridge)
    // ============================================================

    /**
     * @brief Open an emulated device
     * @param path Linux device path (e.g., /dev/lbb, /dev/i2c-*)
     * @param flags Open flags
     * @return File descriptor or -1 on failure
     */
    int Open(const char* path, int flags);

    /**
     * @brief Close an emulated device
     * @param fd File descriptor
     * @return 0 on success, -1 on failure
     */
    int Close(int fd);

    /**
     * @brief Read from an emulated device
     * @param fd File descriptor
     * @param buf Buffer to read into
     * @param count Number of bytes to read
     * @return Number of bytes read or -1 on failure
     */
    int Read(int fd, void* buf, size_t count);

    /**
     * @brief Write to an emulated device
     * @param fd File descriptor
     * @param buf Buffer to write from
     * @param count Number of bytes to write
     * @return Number of bytes written or -1 on failure
     */
    int Write(int fd, const void* buf, size_t count);

    /**
     * @brief Perform ioctl on an emulated device
     * @param fd File descriptor
     * @param request IOCTL request code
     * @param data Request data
     * @return 0 on success, -1 on failure
     */
    int Ioctl(int fd, unsigned long request, void* data);

    /**
     * @brief Seek within an emulated device (for PCI config space)
     * @param fd File descriptor
     * @param offset Seek offset
     * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
     * @return 0 on success, -1 on failure
     */
    int Seek(int fd, long offset, int whence);

    /**
     * @brief Get current position (for PCI config space)
     * @param fd File descriptor
     * @return Current position or -1 on failure
     */
    long Tell(int fd);

    // ============================================================
    // Port I/O Emulation (for VEH handler)
    // ============================================================

    /**
     * @brief Read from an I/O port
     * @param port Port address
     * @param data Output data
     * @return 0 on success
     */
    int PortRead(uint16_t port, uint32_t* data);

    /**
     * @brief Write to an I/O port
     * @param port Port address
     * @param data Data to write
     * @return 0 on success
     */
    int PortWrite(uint16_t port, uint32_t data);

    // ============================================================
    // Hardware Component Accessors
    // ============================================================

    BaseBoard* GetBaseBoard() { return &m_baseBoard; }
    JvsBoard* GetJvsBoard() { return &m_jvsBoard; }
    EepromBoard* GetEepromBoard() { return &m_eepromBoard; }
    SecurityBoard* GetSecurityBoard() { return &m_securityBoard; }
    SerialBoard* GetSerialBoard() { return &m_serialBoard; }

    // ============================================================
    // Convenience Input Methods
    // ============================================================

    void SetSwitch(JVSPlayer player, int switchMask, bool on) {
        m_jvsBoard.SetSwitch(player, switchMask, on);
    }

    void InsertCoin(JVSPlayer player, int amount = 1) {
        m_jvsBoard.IncrementCoin(player, amount);
    }

    void SetAnalogue(int channel, int value) {
        m_jvsBoard.SetAnalogue(channel, value);
    }

private:
    // Private constructor for singleton
    LindberghDevice();
    ~LindberghDevice();

    // Prevent copying
    LindberghDevice(const LindberghDevice&) = delete;
    LindberghDevice& operator=(const LindberghDevice&) = delete;

    // Hardware components
    BaseBoard m_baseBoard;
    JvsBoard m_jvsBoard;
    EepromBoard m_eepromBoard;
    SecurityBoard m_securityBoard;
    SerialBoard m_serialBoard;

    // File descriptor management
    std::map<int, FdInfo> m_fileDescriptors;
    int m_nextFd;
};
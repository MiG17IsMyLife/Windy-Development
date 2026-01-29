#define _CRT_SECURE_NO_WARNINGS

#include "HardwareBridge.h"
#include "../../hardware/lindberghdevice.h"
#include "../../core/log.h"
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

// Linux Open Flags Mapping
#define LINUX_O_RDONLY    00
#define LINUX_O_WRONLY    01
#define LINUX_O_RDWR      02
#define LINUX_O_CREAT     0100
#define LINUX_O_TRUNC     01000
#define LINUX_O_APPEND    02000

static int ConvertFlags(int linuxFlags) {
    int winFlags = _O_BINARY;
    if ((linuxFlags & LINUX_O_RDWR) == LINUX_O_RDWR) winFlags |= _O_RDWR;
    else if ((linuxFlags & LINUX_O_WRONLY) == LINUX_O_WRONLY) winFlags |= _O_WRONLY;
    else winFlags |= _O_RDONLY;

    if (linuxFlags & LINUX_O_CREAT) winFlags |= _O_CREAT;
    if (linuxFlags & LINUX_O_TRUNC) winFlags |= _O_TRUNC;
    if (linuxFlags & LINUX_O_APPEND) winFlags |= _O_APPEND;
    return winFlags;
}

// ========================================================================
// --- Vectored Exception Handler (Port I/O Emulation) ---
// ========================================================================

/**
 * @brief 特権命令(IN/OUT)をトラップしてエミュレーションを行うハンドラ
 */
LONG CALLBACK PortIoVectoredHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    // 0xC0000096: Privileged instruction
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_PRIV_INSTRUCTION) {

        PBYTE eip = (PBYTE)ExceptionInfo->ContextRecord->Eip;
        uint8_t opcode = *eip;

        // 命令で使用されるポート番号(DX)とデータ(EAX)のコンテキストを取得
        uint16_t port = (uint16_t)ExceptionInfo->ContextRecord->Edx;
        uint32_t& eax = (uint32_t&)ExceptionInfo->ContextRecord->Eax;

        bool handled = false;

        switch (opcode) {
        case 0xEC: // IN AL, DX
        case 0xED: // IN EAX, DX
        {
            uint32_t data = 0xFFFFFFFF;
            LindberghDevice::Instance().PortRead(port, &data);

            if (opcode == 0xEC) {
                eax = (eax & 0xFFFFFF00) | (data & 0xFF);
            }
            else {
                eax = data;
            }
            handled = true;
            break;
        }

        case 0xEE: // OUT DX, AL
        case 0xEF: // OUT DX, EAX
        {
            uint32_t data = (opcode == 0xEE) ? (eax & 0xFF) : eax;
            LindberghDevice::Instance().PortWrite(port, data);
            handled = true;
            break;
        }
        }

        if (handled) {
            // 命令を処理したため、命令ポインタを1バイト進めて実行を継続
            ExceptionInfo->ContextRecord->Eip += 1;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void InitHardwareBridge() {
    // 例外ハンドラを登録
    AddVectoredExceptionHandler(1, PortIoVectoredHandler);
    log_info("HardwareBridge: Vectored Exception Handler registered for Port I/O.");

    // デバイス群の初期化
    if (!LindberghDevice::Instance().Init()) {
        log_fatal("Failed to initialize LindberghDevice");
    }
}

extern "C" {

    int my_open(const char* pathname, int flags, int mode) {
        // 1. Lindbergh仮想デバイスへの振り分け
        int fd = LindberghDevice::Instance().Open(pathname, flags);
        if (fd >= 0) {
            return fd;
        }

        // 2. ホストファイルシステムへのフォールバック
        char winPath[MAX_PATH];
        strncpy(winPath, pathname, MAX_PATH);
        winPath[MAX_PATH - 1] = 0;
        for (int i = 0; winPath[i]; i++) {
            if (winPath[i] == '/') winPath[i] = '\\';
        }

        int winFlags = ConvertFlags(flags);
        int pmode = _S_IREAD | _S_IWRITE;

        fd = _open(winPath, winFlags, pmode);
        return fd;
    }

    int my_close(int fd) {
        if (LindberghDevice::Instance().Close(fd) == 0) {
            return 0;
        }
        return _close(fd);
    }

    int my_read(int fd, void* buf, size_t count) {
        int res = LindberghDevice::Instance().Read(fd, buf, count);
        if (res != -1) return res;

        return _read(fd, buf, (unsigned int)count);
    }

    int my_write(int fd, const void* buf, size_t count) {
        int res = LindberghDevice::Instance().Write(fd, buf, count);
        if (res != -1) return res;

        return _write(fd, buf, (unsigned int)count);
    }

    int my_ioctl(int fd, unsigned long request, void* data) {
        int res = LindberghDevice::Instance().Ioctl(fd, request, data);
        if (res != -1) return res;

        // ハンドルされないioctlはエラーを返す
        errno = ENOTTY;
        return -1;
    }

    int my_writev(int fd, const struct iovec* iov, int iovcnt) {
        int total = 0;
        for (int i = 0; i < iovcnt; ++i) {
            int written = my_write(fd, iov[i].iov_base, iov[i].iov_len);
            if (written < 0) return (total > 0) ? total : -1;
            total += written;
        }
        return total;
    }

    int HardwarePortRead(uint16_t port, uint32_t* data) {
        return LindberghDevice::Instance().PortRead(port, data);
    }

    int HardwarePortWrite(uint16_t port, uint32_t data) {
        return LindberghDevice::Instance().PortWrite(port, data);
    }
}
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

// VEH Handle for cleanup
static PVOID g_vehHandle = nullptr;

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
// --- IN/OUT Instruction Decoder ---
// ========================================================================

static int DecodeInInstruction(uint8_t* code, uint16_t* port, int* dataSize, bool* usesDX) {
    int idx = 0;
    bool has66Prefix = false;

    if (code[idx] == 0x66) {
        has66Prefix = true;
        idx++;
    }

    uint8_t opcode = code[idx];

    switch (opcode) {
    case 0xE4: // IN AL, imm8
        *port = code[idx + 1];
        *dataSize = 1;
        *usesDX = false;
        return idx + 2;

    case 0xE5: // IN AX/EAX, imm8
        *port = code[idx + 1];
        *dataSize = has66Prefix ? 2 : 4;
        *usesDX = false;
        return idx + 2;

    case 0xEC: // IN AL, DX
        *port = 0;
        *dataSize = 1;
        *usesDX = true;
        return idx + 1;

    case 0xED: // IN AX/EAX, DX
        *port = 0;
        *dataSize = has66Prefix ? 2 : 4;
        *usesDX = true;
        return idx + 1;

    default:
        return 0; // NOT an IN instruction
    }
}

static int DecodeOutInstruction(uint8_t* code, uint16_t* port, int* dataSize, bool* usesDX) {
    int idx = 0;
    bool has66Prefix = false;

    if (code[idx] == 0x66) {
        has66Prefix = true;
        idx++;
    }

    uint8_t opcode = code[idx];

    switch (opcode) {
    case 0xE6: // OUT imm8, AL
        *port = code[idx + 1];
        *dataSize = 1;
        *usesDX = false;
        return idx + 2;

    case 0xE7: // OUT imm8, AX/EAX
        *port = code[idx + 1];
        *dataSize = has66Prefix ? 2 : 4;
        *usesDX = false;
        return idx + 2;

    case 0xEE: // OUT DX, AL
        *port = 0;
        *dataSize = 1;
        *usesDX = true;
        return idx + 1;

    case 0xEF: // OUT DX, AX/EAX
        *port = 0;
        *dataSize = has66Prefix ? 2 : 4;
        *usesDX = true;
        return idx + 1;

    default:
        return 0;
    }
}

// ========================================================================
// --- Vectored Exception Handler (Port I/O Emulation) ---
// ========================================================================

LONG CALLBACK PortIoVectoredHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    // 0xC0000096: Privileged instruction
    if (ExceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    PCONTEXT ctx = ExceptionInfo->ContextRecord;
    uint8_t* code = (uint8_t*)ctx->Eip;

    uint16_t port;
    int dataSize;
    bool usesDX;
    int instrLen;

    instrLen = DecodeInInstruction(code, &port, &dataSize, &usesDX);
    if (instrLen > 0) {

        if (usesDX) {
            port = (uint16_t)(ctx->Edx & 0xFFFF);
        }

        uint32_t data = 0xFFFFFFFF;
        LindberghDevice::Instance().PortRead(port, &data);

        switch (dataSize) {
        case 1:
            ctx->Eax = (ctx->Eax & 0xFFFFFF00) | (data & 0xFF);
            break;
        case 2:
            ctx->Eax = (ctx->Eax & 0xFFFF0000) | (data & 0xFFFF);
            break;
        case 4:
            ctx->Eax = data;
            break;
        }

        ctx->Eip += instrLen;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    instrLen = DecodeOutInstruction(code, &port, &dataSize, &usesDX);
    if (instrLen > 0) {
        if (usesDX) {
            port = (uint16_t)(ctx->Edx & 0xFFFF);
        }

        uint32_t data;
        switch (dataSize) {
        case 1:
            data = ctx->Eax & 0xFF;
            break;
        case 2:
            data = ctx->Eax & 0xFFFF;
            break;
        default:
            data = ctx->Eax;
            break;
        }

        LindberghDevice::Instance().PortWrite(port, data);

        ctx->Eip += instrLen;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    log_error("VEH: Unhandled privileged instruction at 0x%08X: %02X %02X %02X %02X",
        ctx->Eip, code[0], code[1], code[2], code[3]);

    return EXCEPTION_CONTINUE_SEARCH;
}

// ========================================================================
// --- Initialization / Cleanup ---
// ========================================================================

void InitHardwareBridge() {
    g_vehHandle = AddVectoredExceptionHandler(1, PortIoVectoredHandler);
    if (g_vehHandle) {
        log_info("HardwareBridge: Vectored Exception Handler registered for Port I/O.");
    }
    else {
        log_error("HardwareBridge: Failed to register VEH!");
    }

    if (!LindberghDevice::Instance().Init()) {
        log_fatal("Failed to initialize LindberghDevice");
    }
}

void CleanupHardwareBridge() {
    if (g_vehHandle) {
        RemoveVectoredExceptionHandler(g_vehHandle);
        g_vehHandle = nullptr;
        log_info("HardwareBridge: VEH removed.");
    }
}

// ========================================================================
// --- File I/O Functions (with debug logging)
// ========================================================================

extern "C" {

    int my_open(const char* pathname, int flags, int mode) {
        log_debug(">>> my_open ENTRY: path=%s, flags=0x%X", pathname, flags);

        int fd = LindberghDevice::Instance().Open(pathname, flags);
        if (fd >= 0) {
            log_debug(">>> my_open EXIT: virtual fd=%d", fd);
            return fd;
        }

        char winPath[MAX_PATH];
        strncpy(winPath, pathname, MAX_PATH);
        winPath[MAX_PATH - 1] = 0;
        for (int i = 0; winPath[i]; i++) {
            if (winPath[i] == '/') winPath[i] = '\\';
        }

        int winFlags = ConvertFlags(flags);
        int pmode = _S_IREAD | _S_IWRITE;

        fd = _open(winPath, winFlags, pmode);
        log_debug(">>> my_open EXIT: real fd=%d for %s", fd, winPath);
        return fd;
    }

    int my_close(int fd) {
        log_debug(">>> my_close ENTRY: fd=%d", fd);

        if (LindberghDevice::Instance().Close(fd) == 0) {
            log_debug(">>> my_close EXIT: virtual fd closed");
            return 0;
        }

        int ret = _close(fd);
        log_debug(">>> my_close EXIT: ret=%d", ret);
        return ret;
    }

    int my_read(int fd, void* buf, size_t count) {
        log_debug(">>> my_read ENTRY: fd=%d, count=%zu", fd, count);

        int res = LindberghDevice::Instance().Read(fd, buf, count);
        if (res != -1) {
            log_debug(">>> my_read EXIT: virtual read returned %d", res);
            return res;
        }

        res = _read(fd, buf, (unsigned int)count);
        log_debug(">>> my_read EXIT: real read returned %d", res);
        return res;
    }

    int my_write(int fd, const void* buf, size_t count) {
        log_debug(">>> my_write ENTRY: fd=%d, count=%zu", fd, count);

        int res = LindberghDevice::Instance().Write(fd, buf, count);
        if (res != -1) {
            log_debug(">>> my_write EXIT: virtual write returned %d", res);
            return res;
        }

        res = _write(fd, buf, (unsigned int)count);
        log_debug(">>> my_write EXIT: real write returned %d", res);
        return res;
    }

    int my_ioctl(int fd, unsigned long request, void* data) {
        log_debug(">>> my_ioctl ENTRY: fd=%d, request=0x%lX", fd, request);

        int res = LindberghDevice::Instance().Ioctl(fd, request, data);
        if (res != -1) {
            log_debug(">>> my_ioctl EXIT: virtual ioctl returned %d", res);
            return res;
        }

        log_debug(">>> my_ioctl EXIT: returning -1 (ENOTTY)");
        errno = ENOTTY;
        return -1;
    }

    int my_writev(int fd, const struct iovec* iov, int iovcnt) {
        log_debug(">>> my_writev ENTRY: fd=%d, iovcnt=%d", fd, iovcnt);

        int total = 0;
        for (int i = 0; i < iovcnt; ++i) {
            int written = my_write(fd, iov[i].iov_base, iov[i].iov_len);
            if (written < 0) {
                log_debug(">>> my_writev EXIT: error, total=%d", total);
                return (total > 0) ? total : -1;
            }
            total += written;
        }

        log_debug(">>> my_writev EXIT: total=%d", total);
        return total;
    }

    int HardwarePortRead(uint16_t port, uint32_t* data) {
        return LindberghDevice::Instance().PortRead(port, data);
    }

    int HardwarePortWrite(uint16_t port, uint32_t data) {
        return LindberghDevice::Instance().PortWrite(port, data);
    }
}
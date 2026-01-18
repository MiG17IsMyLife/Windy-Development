#pragma once

#include "LinuxTypes.h"

// =============================================================
//   Filesystem & Dynamic Loader Emulation (extern "C")
// =============================================================
extern "C" {

    // ---------------------------------------------------------
    // Dynamic Linking (dlfcn.h)
    // ---------------------------------------------------------
    void* my_dlopen(const char* filename, int flags);
    void* my_dlsym(void* handle, const char* symbol);
    int my_dlclose(void* handle);
    char* my_dlerror(void);

    // ---------------------------------------------------------
    // Directory Operations (dirent.h)
    // ---------------------------------------------------------
    void* my_opendir(const char* name);
    struct linux_dirent* my_readdir(void* dirp);
    int my_closedir(void* dirp);
    void my_rewinddir(void* dirp);

    // ---------------------------------------------------------
    // File Status / Info (sys/stat.h)
    // ---------------------------------------------------------
    // Note: These will be implemented in FilesystemBridge.cpp
    // to map Windows _stat64 to Linux stat64 structures.

    int my_stat(const char* path, struct linux_stat64* buf);
    int my_fstat(int fd, struct linux_stat64* buf);
    int my_lstat(const char* path, struct linux_stat64* buf);

    // Glibc internal stat versions (ver argument is usually 3)
    int __xstat(int ver, const char* path, struct linux_stat64* buf);
    int __lxstat(int ver, const char* path, struct linux_stat64* buf);
    int __fxstat(int ver, int fd, struct linux_stat64* buf);
    int __fxstat64(int ver, int fd, struct linux_stat64* buf);

    // ---------------------------------------------------------
    // File Control (fcntl.h)
    // ---------------------------------------------------------
    int my_fcntl(int fd, int cmd, ...);
}
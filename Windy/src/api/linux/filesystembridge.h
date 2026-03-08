#pragma once

#include "linuxtypes.h"

// =============================================================
//   Filesystem & Dynamic Loader Emulation (extern "C")
// =============================================================
extern "C"
{

    // ---------------------------------------------------------
    // Dynamic Linking (dlfcn.h)
    // ---------------------------------------------------------
    void *my_dlopen(const char *filename, int flags);
    void *my_dlsym(void *handle, const char *symbol);
    int my_dlclose(void *handle);
    char *my_dlerror(void);

    // ---------------------------------------------------------
    // Directory Operations (dirent.h)
    // ---------------------------------------------------------
    void *my_opendir(const char *name);
    struct linux_dirent *my_readdir(void *dirp);
    int my_closedir(void *dirp);
    void my_rewinddir(void *dirp);
    int my_mkdir(const char *pathname, mode_t mode);

    // ---------------------------------------------------------
    // File Open / At
    // ---------------------------------------------------------
    int my_openat(int dirfd, const char *pathname, int flags, ...);

    // ---------------------------------------------------------
    // File Status / Info (sys/stat.h)
    // ---------------------------------------------------------
    int my_stat(const char *path, struct linux_stat64 *buf);
    int my_fstat(int fd, struct linux_stat64 *buf);
    int my_lstat(const char *path, struct linux_stat64 *buf);

    // Glibc internal stat versions (ver argument is usually 3)
    int fs_xstat(int ver, const char *path, struct linux_stat *buf);
    int fs_lxstat(int ver, const char *path, struct linux_stat *buf);
    int fs_fxstat(int ver, int fd, struct linux_stat *buf);
    int fs_fxstat64(int ver, int fd, struct linux_stat64 *buf);
    int fs_xstat64(int ver, const char *path, struct linux_stat64 *buf);

    int my_readlink(const char *path, char *buf, size_t bufsiz);

    // ---------------------------------------------------------
    // File Control (fcntl.h)
    // ---------------------------------------------------------
    int my_fcntl(int fd, int cmd, ...);
}
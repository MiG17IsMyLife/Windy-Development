#pragma once

#include <stdint.h>

// =============================================================
//   Missing Types for MSVC (Windows)
// =============================================================

#ifndef MY_MODE_T_DEFINED
typedef unsigned short mode_t;
#define MY_MODE_T_DEFINED
#endif

#ifndef MY_DEV_T_DEFINED
typedef unsigned int dev_t;
#define MY_DEV_T_DEFINED
#endif

#ifndef MY_PID_T_DEFINED
typedef int pid_t;
#define MY_PID_T_DEFINED
#endif

#ifndef MY_UID_T_DEFINED
typedef unsigned int uid_t;
#define MY_UID_T_DEFINED
#endif

#ifndef MY_GID_T_DEFINED
typedef unsigned int gid_t;
#define MY_GID_T_DEFINED
#endif

// =============================================================
//   Linux (i386/32-bit) Structure Definitions
// =============================================================

// dirent structure (for getdents/readdir)
struct linux_dirent
{
    long d_ino;
    long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

// stat64 structure (for stat64/fstat64/lstat64)
// Layout must match Linux i386 glibc 'struct stat64'
struct linux_stat64
{
    unsigned long long st_dev;
    unsigned char __pad0[4];
    unsigned long __st_ino;
    unsigned int st_mode;
    unsigned int st_nlink;
    unsigned long st_uid;
    unsigned long st_gid;
    unsigned long long st_rdev;
    unsigned char __pad3[4];
    long long st_size;
    unsigned long st_blksize;
    unsigned long long st_blocks;
    unsigned long st_atime;
    unsigned long st_atime_nsec;
    unsigned long st_mtime;
    unsigned long st_mtime_nsec;
    unsigned long st_ctime;
    unsigned long st_ctime_nsec;
    unsigned long long st_ino;
};

// iovec structure (for writev/readv)
struct iovec
{
    void *iov_base; // Starting address
    size_t iov_len; // Number of bytes to transfer
};

// =============================================================
//   File Type Flags (st_mode)
// =============================================================
#define LINUX_S_IFMT 0170000
#define LINUX_S_IFSOCK 0140000
#define LINUX_S_IFLNK 0120000
#define LINUX_S_IFREG 0100000
#define LINUX_S_IFBLK 0060000
#define LINUX_S_IFDIR 0040000
#define LINUX_S_IFCHR 0020000
#define LINUX_S_IFIFO 0010000

// =============================================================
//   Support for Select (fd_set)
// =============================================================
#ifndef LINUX_FD_SETSIZE
#define LINUX_FD_SETSIZE 1024
#endif

#define LINUX_NFDBITS (8 * sizeof(unsigned long))

typedef struct
{
    unsigned long fds_bits[LINUX_FD_SETSIZE / LINUX_NFDBITS];
} linux_fd_set;
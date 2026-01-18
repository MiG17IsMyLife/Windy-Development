#define _CRT_SECURE_NO_WARNINGS

// =============================================================
//   System Headers (MUST be included first)
// =============================================================
#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>

#include <iostream>
#include <string>
#include <vector>

// =============================================================
//   Bridge Headers
// =============================================================
#include "FilesystemBridge.h"

// =============================================================
//   Internal Structures
// =============================================================

struct DIR_Impl {
    HANDLE hFind;
    WIN32_FIND_DATAA findData;
    struct linux_dirent ent;
    bool first_read;
    bool finished;
    std::string path;
};

static char g_dlErrorBuf[256];

// =============================================================
//   Helpers
// =============================================================

static void CopyStat(const struct _stat64& src, struct linux_stat64* dst) {
    memset(dst, 0, sizeof(struct linux_stat64));

    dst->st_dev = (unsigned long long)src.st_dev;
    dst->st_ino = (unsigned long long)src.st_ino;
    dst->st_nlink = src.st_nlink;
    dst->st_uid = src.st_uid;
    dst->st_gid = src.st_gid;
    dst->st_rdev = (unsigned long long)src.st_rdev;
    dst->st_size = src.st_size;
    dst->st_atime = (unsigned long)src.st_atime;
    dst->st_mtime = (unsigned long)src.st_mtime;
    dst->st_ctime = (unsigned long)src.st_ctime;

    dst->st_mode = 0;
    if (src.st_mode & _S_IFDIR) dst->st_mode |= LINUX_S_IFDIR;
    if (src.st_mode & _S_IFREG) dst->st_mode |= LINUX_S_IFREG;
    if (src.st_mode & _S_IFCHR) dst->st_mode |= LINUX_S_IFCHR;
    if (src.st_mode & _S_IFIFO) dst->st_mode |= LINUX_S_IFIFO;

    dst->st_mode |= (src.st_mode & 0777);
}

// =============================================================
//   Implementation (extern "C")
// =============================================================
extern "C" {

    // ---------------------------------------------------------
    // Dynamic Linking (dlfcn.h)
    // ---------------------------------------------------------

    void* my_dlopen(const char* filename, int flags) {
        std::cout << "[Loader] dlopen called: " << (filename ? filename : "NULL") << std::endl;

        if (!filename) return GetModuleHandle(NULL);

        std::string path = filename;

        if (path.find("libCg.so") != std::string::npos) path = "cg.dll";
        else if (path.find("libCgGL.so") != std::string::npos) path = "cgGL.dll";
        else if (path.find("libGL.so") != std::string::npos) path = "opengl32.dll";
        else if (path.find("libGLU.so") != std::string::npos) path = "glu32.dll";
        else if (path.find("libglut") != std::string::npos) path = "freeglut.dll";

        HMODULE hLib = LoadLibraryA(path.c_str());
        if (!hLib) {
            sprintf(g_dlErrorBuf, "LoadLibrary failed for %s (Error: %lu)", path.c_str(), GetLastError());
            std::cerr << "[Loader] dlopen failed: " << g_dlErrorBuf << std::endl;
            return NULL;
        }
        return (void*)hLib;
    }

    void* my_dlsym(void* handle, const char* symbol) {
        if (!handle) return NULL;

        void* proc = (void*)GetProcAddress((HMODULE)handle, symbol);
        if (!proc) {
            sprintf(g_dlErrorBuf, "GetProcAddress failed for %s", symbol);
        }
        return proc;
    }

    int my_dlclose(void* handle) {
        if (handle) FreeLibrary((HMODULE)handle);
        return 0;
    }

    char* my_dlerror(void) {
        return g_dlErrorBuf;
    }

    // ---------------------------------------------------------
    // Directory Operations (dirent.h)
    // ---------------------------------------------------------

    void* my_opendir(const char* name) {
        std::cout << "[FS] opendir called: " << name << std::endl;

        std::string searchPath = name;
        if (searchPath.empty()) return NULL;

        if (searchPath.back() == '/' || searchPath.back() == '\\') {
            searchPath += "*";
        }
        else {
            searchPath += "\\*";
        }

        DIR_Impl* dir = new DIR_Impl();
        dir->path = name;
        dir->hFind = FindFirstFileA(searchPath.c_str(), &dir->findData);

        if (dir->hFind == INVALID_HANDLE_VALUE) {
            delete dir;
            return NULL;
        }

        dir->first_read = true;
        dir->finished = false;
        return (void*)dir;
    }

    struct linux_dirent* my_readdir(void* dirp) {
        if (!dirp) return NULL;
        DIR_Impl* dir = (DIR_Impl*)dirp;

        if (dir->finished) return NULL;

        if (dir->first_read) {
            dir->first_read = false;
        }
        else {
            if (!FindNextFileA(dir->hFind, &dir->findData)) {
                dir->finished = true;
                return NULL;
            }
        }

        // Copy to linux direct structure
        memset(&dir->ent, 0, sizeof(linux_dirent));
        dir->ent.d_ino = 1; // dummy indore number
        dir->ent.d_off = 0;

        // copy file name
        strncpy(dir->ent.d_name, dir->findData.cFileName, 255);
        dir->ent.d_reclen = (unsigned short)strlen(dir->ent.d_name);

        // d_type setting
        if (dir->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dir->ent.d_type = 4; // DT_DIR
        }
        else {
            dir->ent.d_type = 8; // DT_REG
        }

        return &dir->ent;
    }

    int my_closedir(void* dirp) {
        if (!dirp) return -1;
        DIR_Impl* dir = (DIR_Impl*)dirp;
        if (dir->hFind != INVALID_HANDLE_VALUE) {
            FindClose(dir->hFind);
        }
        delete dir;
        return 0;
    }

    void my_rewinddir(void* dirp) {
        if (!dirp) return;
        DIR_Impl* dir = (DIR_Impl*)dirp;

        if (dir->hFind != INVALID_HANDLE_VALUE) {
            FindClose(dir->hFind);
        }

        std::string searchPath = dir->path;
        if (searchPath.back() == '/' || searchPath.back() == '\\') {
            searchPath += "*";
        }
        else {
            searchPath += "\\*";
        }

        dir->hFind = FindFirstFileA(searchPath.c_str(), &dir->findData);
        dir->first_read = true;
        dir->finished = (dir->hFind == INVALID_HANDLE_VALUE);
    }

    // ---------------------------------------------------------
    // File Status / Info (stat family)
    // ---------------------------------------------------------

    int my_stat(const char* path, struct linux_stat64* buf) {
        struct _stat64 win_stat;
        if (_stat64(path, &win_stat) != 0) return -1;
        CopyStat(win_stat, buf);
        return 0;
    }

    int my_fstat(int fd, struct linux_stat64* buf) {
        struct _stat64 win_stat;
        if (_fstat64(fd, &win_stat) != 0) return -1;
        CopyStat(win_stat, buf);
        return 0;
    }

    int my_lstat(const char* path, struct linux_stat64* buf) {
        return my_stat(path, buf);
    }

    int __xstat(int ver, const char* path, struct linux_stat64* buf) {
        return my_stat(path, buf);
    }

    int __lxstat(int ver, const char* path, struct linux_stat64* buf) {
        return my_lstat(path, buf);
    }

    int __fxstat(int ver, int fd, struct linux_stat64* buf) {
        return my_fstat(fd, buf);
    }

    int __fxstat64(int ver, int fd, struct linux_stat64* buf) {
        return my_fstat(fd, buf);
    }

    // ---------------------------------------------------------
    // File Control (fcntl)
    // ---------------------------------------------------------
    int my_fcntl(int fd, int cmd, ...) {
        // lazy implementation
        return 0;
    }
}
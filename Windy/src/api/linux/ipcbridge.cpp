#define _CRT_SECURE_NO_WARNINGS
#include "IpcBridge.h"
#include <windows.h>
#include <iostream>
#include <map>
#include <stdio.h>

// =============================================================
//   Internal Structures & Globals
// =============================================================

struct ShmInfo {
    HANDLE hMap;    // Windows mapping handle
    void* pMem;     // Mapped memory address
    size_t size;    // Size
    int key;        // Linux shm key
};

static std::map<int, ShmInfo> g_shmMap;

// =============================================================
//   Implementation (extern "C")
// =============================================================
extern "C" {

    // ----------------------------------------------------------------
    // shmget: Allocates a shared memory segment
    // ----------------------------------------------------------------
    int my_shmget(int key, size_t size, int shmflg) {
        std::cout << "[IPC] shmget called. Key: " << key << ", Size: " << size << std::endl;

        if (g_shmMap.find(key) != g_shmMap.end()) {
            return key;
        }

        char mapName[64];
        sprintf(mapName, "Global\\Windy_SHM_%d", key);

        HANDLE hMap = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            (DWORD)size,
            mapName);

        if (!hMap) {
            std::cerr << "[IPC] shmget failed: CreateFileMapping error " << GetLastError() << std::endl;
            return -1;
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            // std::cout << "[IPC] shmget: Opened existing mapping." << std::endl;
        }

        ShmInfo info;
        info.hMap = hMap;
        info.pMem = NULL;
        info.size = size;
        info.key = key;

        g_shmMap[key] = info;

        return key; // another lazy implemention 
    }

    // ----------------------------------------------------------------
    // shmat: Attaches the shared memory segment
    // ----------------------------------------------------------------
    void* my_shmat(int shmid, const void* shmaddr, int shmflg) {
        // std::cout << "[IPC] shmat called. ID: " << shmid << std::endl;

        if (g_shmMap.find(shmid) == g_shmMap.end()) {
            std::cerr << "[IPC] shmat failed: Invalid shmid " << shmid << std::endl;
            return (void*)-1;
        }

        ShmInfo& info = g_shmMap[shmid];

        if (info.pMem) {
            return info.pMem;
        }

        info.pMem = MapViewOfFile(
            info.hMap,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            0
        );

        if (!info.pMem) {
            std::cerr << "[IPC] shmat failed: MapViewOfFile error " << GetLastError() << std::endl;
            return (void*)-1;
        }

        return info.pMem;
    }

    // ----------------------------------------------------------------
    // shmctl: Shared memory control operations
    // ----------------------------------------------------------------
    int my_shmctl(int shmid, int cmd, void* buf) {
        if (cmd == 0) {
            std::cout << "[IPC] shmctl: Removing ID " << shmid << std::endl;

            if (g_shmMap.find(shmid) != g_shmMap.end()) {
                ShmInfo& info = g_shmMap[shmid];

                if (info.pMem) {
                    UnmapViewOfFile(info.pMem);
                }

                if (info.hMap) {
                    CloseHandle(info.hMap);
                }

                g_shmMap.erase(shmid);
            }
        }

        return 0;
    }

    // ----------------------------------------------------------------
    // shmdt: Detaches the shared memory segment
    // ----------------------------------------------------------------
    int my_shmdt(const void* shmaddr) {
        return 0;
    }
}
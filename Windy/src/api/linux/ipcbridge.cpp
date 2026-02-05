#define _CRT_SECURE_NO_WARNINGS
#include "ipcbridge.h"
#include "../../core/log.h"
#include <windows.h>
#include <iostream>
#include <map>
#include <stdio.h>

// =============================================================
//   Internal Structures & Globals
// =============================================================

struct ShmInfo
{
    HANDLE hMap;
    void *pMem;
    size_t size;
    int key;
};

static std::map<int, ShmInfo> g_shmMap;

// =============================================================
//   Implementation (extern "C")
// =============================================================
extern "C"
{

    int my_shmget(int key, size_t size, int shmflg)
    {
        log_debug("shmget(key=%d, size=%zu, flags=0x%X)", key, size, shmflg);

        if (g_shmMap.find(key) != g_shmMap.end())
        {
            log_trace("shmget: Returning existing key %d", key);
            return key;
        }

        char mapName[64];
        sprintf(mapName, "Global\\Windy_SHM_%d", key);

        HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)size, mapName);

        if (!hMap)
        {
            log_error("shmget failed: CreateFileMapping error %lu", GetLastError());
            return -1;
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            log_trace("shmget: Opened existing mapping for key %d", key);
        }

        ShmInfo info;
        info.hMap = hMap;
        info.pMem = NULL;
        info.size = size;
        info.key = key;

        g_shmMap[key] = info;

        log_debug("shmget: Created mapping for key %d, size %zu", key, size);
        return key;
    }

    void *my_shmat(int shmid, const void *shmaddr, int shmflg)
    {
        log_trace("shmat(id=%d, addr=%p, flags=0x%X)", shmid, shmaddr, shmflg);

        if (g_shmMap.find(shmid) == g_shmMap.end())
        {
            log_error("shmat failed: Invalid shmid %d", shmid);
            return (void *)-1;
        }

        ShmInfo &info = g_shmMap[shmid];

        if (info.pMem)
        {
            return info.pMem;
        }

        info.pMem = MapViewOfFile(info.hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);

        if (!info.pMem)
        {
            log_error("shmat failed: MapViewOfFile error %lu", GetLastError());
            return (void *)-1;
        }

        log_debug("shmat: Attached id=%d at %p", shmid, info.pMem);
        return info.pMem;
    }

    int my_shmctl(int shmid, int cmd, void *buf)
    {
        if (cmd == 0)
        { // IPC_RMID
            log_debug("shmctl: Removing id=%d", shmid);

            if (g_shmMap.find(shmid) != g_shmMap.end())
            {
                ShmInfo &info = g_shmMap[shmid];

                if (info.pMem)
                {
                    UnmapViewOfFile(info.pMem);
                }

                if (info.hMap)
                {
                    CloseHandle(info.hMap);
                }

                g_shmMap.erase(shmid);
            }
        }

        return 0;
    }

    int my_shmdt(const void *shmaddr)
    {
        log_trace("shmdt(%p)", shmaddr);
        return 0;
    }
}
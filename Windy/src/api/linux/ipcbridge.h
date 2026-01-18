#pragma once

#include <stddef.h>

// =============================================================
//   IPC / Shared Memory Emulation (extern "C")
// =============================================================
extern "C" {
    // Shared Memory Get
    // key: Shared memory identifier (key_t)
    // size: Size of the memory segment
    // shmflg: Creation flags (e.g., IPC_CREAT)
    int my_shmget(int key, size_t size, int shmflg);

    // Shared Memory Attach
    // shmid: ID returned by shmget
    // shmaddr: Preferred address (usually NULL)
    // shmflg: Operation flags (e.g., SHM_RDONLY)
    void* my_shmat(int shmid, const void* shmaddr, int shmflg);

    // Shared Memory Control
    // shmid: ID returned by shmget
    // cmd: Command (e.g., IPC_RMID to remove)
    // buf: Buffer for status info (struct shmid_ds*)
    int my_shmctl(int shmid, int cmd, void* buf);

    // Shared Memory Detach
    // shmaddr: Address returned by shmat
    int my_shmdt(const void* shmaddr);
}

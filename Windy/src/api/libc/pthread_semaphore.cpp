// ============================================================
// pthread_semaphore.cpp - Semaphore Emulation
// ============================================================

#include "pthread_emu.h"
#include "pthread_internal.h"

// ============================================================
// Semaphore Internal Structure
// ============================================================

typedef struct SemaphoreInternal {
    HANDLE              handle;
    volatile LONG       value;
    volatile int        initialized;
} SemaphoreInternal;

#define SEM_INIT_MAGIC  0x53454D41  // "SEMA"
#define SEM_VALUE_MAX   0x7FFFFFFF

// ============================================================
// Semaphore Mapping (similar to mutex mapping)
// ============================================================

#include <unordered_map>

static std::unordered_map<void*, SemaphoreInternal*> s_sem_map;
static SRWLOCK s_sem_lock = SRWLOCK_INIT;

static SemaphoreInternal* GetOrCreateSem(void* linux_sem, unsigned int initial_value = 0) {
    AcquireSRWLockExclusive(&s_sem_lock);
    
    auto it = s_sem_map.find(linux_sem);
    if (it != s_sem_map.end() && it->second->initialized == SEM_INIT_MAGIC) {
        ReleaseSRWLockExclusive(&s_sem_lock);
        return it->second;
    }
    
    SemaphoreInternal* sem = new SemaphoreInternal;
    sem->handle = CreateSemaphoreA(NULL, initial_value, SEM_VALUE_MAX, NULL);
    sem->value = initial_value;
    sem->initialized = SEM_INIT_MAGIC;
    
    if (!sem->handle) {
        delete sem;
        ReleaseSRWLockExclusive(&s_sem_lock);
        return nullptr;
    }
    
    s_sem_map[linux_sem] = sem;
    
    ReleaseSRWLockExclusive(&s_sem_lock);
    return sem;
}

static SemaphoreInternal* FindSem(void* linux_sem) {
    AcquireSRWLockShared(&s_sem_lock);
    
    auto it = s_sem_map.find(linux_sem);
    SemaphoreInternal* result = nullptr;
    if (it != s_sem_map.end() && it->second->initialized == SEM_INIT_MAGIC) {
        result = it->second;
    }
    
    ReleaseSRWLockShared(&s_sem_lock);
    return result;
}

static void DestroySem(void* linux_sem) {
    AcquireSRWLockExclusive(&s_sem_lock);
    
    auto it = s_sem_map.find(linux_sem);
    if (it != s_sem_map.end()) {
        if (it->second) {
            if (it->second->handle) {
                CloseHandle(it->second->handle);
            }
            it->second->initialized = 0;
            delete it->second;
        }
        s_sem_map.erase(it);
    }
    
    ReleaseSRWLockExclusive(&s_sem_lock);
}

// ============================================================
// Semaphore API - C Functions
// ============================================================

extern "C" {

int emu_sem_init(void* sem, int pshared, unsigned int value) {
    if (!sem) return -1;
    
    // pshared != 0 is not supported on Windows
    if (pshared != 0) {
        return -1;  // ENOSYS equivalent
    }
    
    SemaphoreInternal* s = GetOrCreateSem(sem, value);
    if (!s) return -1;
    
    return 0;
}

int emu_sem_destroy(void* sem) {
    if (!sem) return -1;
    
    DestroySem(sem);
    return 0;
}

int emu_sem_wait(void* sem) {
    if (!sem) return -1;
    
    SemaphoreInternal* s = FindSem(sem);
    if (!s) {
        // Auto-initialize with value 0
        s = GetOrCreateSem(sem, 0);
        if (!s) return -1;
    }
    
    DWORD result = WaitForSingleObject(s->handle, INFINITE);
    if (result == WAIT_OBJECT_0) {
        InterlockedDecrement(&s->value);
        return 0;
    }
    
    return -1;
}

int emu_sem_trywait(void* sem) {
    if (!sem) return -1;
    
    SemaphoreInternal* s = FindSem(sem);
    if (!s) return -1;
    
    DWORD result = WaitForSingleObject(s->handle, 0);
    if (result == WAIT_OBJECT_0) {
        InterlockedDecrement(&s->value);
        return 0;
    }
    
    // EAGAIN - would block
    return -1;
}

int emu_sem_timedwait(void* sem, const struct timespec* abs_timeout) {
    if (!sem) return -1;
    
    SemaphoreInternal* s = FindSem(sem);
    if (!s) return -1;
    
    DWORD timeout_ms = INFINITE;
    if (abs_timeout) {
        timeout_ms = TimespecToMilliseconds(abs_timeout);
    }
    
    DWORD result = WaitForSingleObject(s->handle, timeout_ms);
    if (result == WAIT_OBJECT_0) {
        InterlockedDecrement(&s->value);
        return 0;
    }
    
    if (result == WAIT_TIMEOUT) {
        // ETIMEDOUT
        return -1;
    }
    
    return -1;
}

int emu_sem_post(void* sem) {
    if (!sem) return -1;
    
    SemaphoreInternal* s = FindSem(sem);
    if (!s) return -1;
    
    InterlockedIncrement(&s->value);
    
    if (!ReleaseSemaphore(s->handle, 1, NULL)) {
        InterlockedDecrement(&s->value);
        return -1;
    }
    
    return 0;
}

int emu_sem_getvalue(void* sem, int* sval) {
    if (!sem || !sval) return -1;
    
    SemaphoreInternal* s = FindSem(sem);
    if (!s) return -1;
    
    *sval = (int)s->value;
    return 0;
}

} // extern "C"

// ============================================================
// Cleanup function for semaphores
// ============================================================

void SemaphoreCleanup() {
    AcquireSRWLockExclusive(&s_sem_lock);
    
    for (auto& pair : s_sem_map) {
        if (pair.second) {
            if (pair.second->handle) {
                CloseHandle(pair.second->handle);
            }
            delete pair.second;
        }
    }
    s_sem_map.clear();
    
    ReleaseSRWLockExclusive(&s_sem_lock);
}

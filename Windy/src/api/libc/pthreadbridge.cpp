#define _CRT_SECURE_NO_WARNINGS

#include "PthreadBridge.h"
#include <cstring>
#include <iostream>
#include <map>

#include "../src/core/log.h"

static std::map<void*, pthread_mutex_t*> g_mutexMap;
static std::map<void*, pthread_cond_t*>  g_condMap;
static std::map<uint32_t, pthread_t>     g_threadMap;
static uint32_t                          g_nextThreadId = 1000;

// Global lock for thread-safe map access
static pthread_mutex_t g_mapLock = PTHREAD_MUTEX_INITIALIZER;
static bool g_mapLockInitialized = false;

static void EnsureMapLockInitialized() {
    if (!g_mapLockInitialized) {
        pthread_mutex_init(&g_mapLock, NULL);
        g_mapLockInitialized = true;
    }
}

// ============================================================
// Mutex Mapping Helpers
// ============================================================

static pthread_mutex_t* GetOrCreateMutex(void* linuxMutex) {
    EnsureMapLockInitialized();
    pthread_mutex_lock(&g_mapLock);

    auto it = g_mutexMap.find(linuxMutex);
    if (it != g_mutexMap.end()) {
        pthread_mutex_unlock(&g_mapLock);
        return it->second;
    }

    // Create new Windows mutex with recursive attribute
    pthread_mutex_t* winMutex = new pthread_mutex_t;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(winMutex, &attr);
    pthread_mutexattr_destroy(&attr);

    g_mutexMap[linuxMutex] = winMutex;
    log_debug("PthreadBridge: Created mutex mapping %p -> %p", linuxMutex, winMutex);

    pthread_mutex_unlock(&g_mapLock);
    return winMutex;
}

static pthread_mutex_t* FindMutex(void* linuxMutex) {
    EnsureMapLockInitialized();
    pthread_mutex_lock(&g_mapLock);

    auto it = g_mutexMap.find(linuxMutex);
    pthread_mutex_t* result = (it != g_mutexMap.end()) ? it->second : nullptr;

    pthread_mutex_unlock(&g_mapLock);
    return result;
}

// ============================================================
// Condition Variable Mapping Helpers
// ============================================================

static pthread_cond_t* GetOrCreateCond(void* linuxCond) {
    EnsureMapLockInitialized();
    pthread_mutex_lock(&g_mapLock);

    auto it = g_condMap.find(linuxCond);
    if (it != g_condMap.end()) {
        pthread_mutex_unlock(&g_mapLock);
        return it->second;
    }

    // Create new Windows condition variable
    pthread_cond_t* winCond = new pthread_cond_t;
    pthread_cond_init(winCond, NULL);

    g_condMap[linuxCond] = winCond;
    log_debug("PthreadBridge: Created cond mapping %p -> %p", linuxCond, winCond);

    pthread_mutex_unlock(&g_mapLock);
    return winCond;
}

// ============================================================
// Mutex Wrappers
// ============================================================

int PthreadBridge::mutex_init_wrapper(void* mutex, const void* attr) {
    log_debug("PthreadBridge: mutex_init %p", mutex);
    GetOrCreateMutex(mutex);
    return 0;
}

int PthreadBridge::mutex_lock_wrapper(void* mutex) {
    pthread_mutex_t* winMutex = GetOrCreateMutex(mutex);

    log_debug(">>> mutex_lock: %p", mutex);

    int ret = pthread_mutex_lock(winMutex);

    if (ret != 0) {
        log_error("PthreadBridge: mutex_lock failed for %p (error: %d)", mutex, ret);
    }

    return ret;
}

int PthreadBridge::mutex_trylock_wrapper(void* mutex) {
    pthread_mutex_t* winMutex = GetOrCreateMutex(mutex);
    log_debug(">>> mutex_trylock: %p", mutex);
    return pthread_mutex_trylock(winMutex);
}

int PthreadBridge::mutex_unlock_wrapper(void* mutex) {
    pthread_mutex_t* winMutex = FindMutex(mutex);

    if (!winMutex) {
        log_warn("PthreadBridge: mutex_unlock called on unknown mutex %p", mutex);
        return 0;  // Return success to avoid breaking the app
    }

    log_debug(">>> mutex_unlock: %p", mutex);
    return pthread_mutex_unlock(winMutex);
}

int PthreadBridge::mutex_destroy_wrapper(void* mutex) {
    EnsureMapLockInitialized();
    pthread_mutex_lock(&g_mapLock);

    auto it = g_mutexMap.find(mutex);
    if (it != g_mutexMap.end()) {
        pthread_mutex_destroy(it->second);
        delete it->second;
        g_mutexMap.erase(it);
        log_debug("PthreadBridge: Destroyed mutex %p", mutex);
    }

    pthread_mutex_unlock(&g_mapLock);
    return 0;
}

// ============================================================
// Thread Wrappers
// ============================================================

int PthreadBridge::create_wrapper(void* thread, const void* attr, void* (*start_routine)(void*), void* arg) {
    EnsureMapLockInitialized();

    pthread_t wthread;
    int ret = pthread_create(&wthread, NULL, start_routine, arg);

    if (ret == 0 && thread) {
        pthread_mutex_lock(&g_mapLock);

        // Assign a simple incrementing ID for Linux side
        uint32_t tid = g_nextThreadId++;
        g_threadMap[tid] = wthread;

        // Write the ID to Linux pthread_t location (32-bit)
        memcpy(thread, &tid, sizeof(uint32_t));

        pthread_mutex_unlock(&g_mapLock);

        log_info("PthreadBridge: Created thread, linux_tid=%u", tid);
    }
    else if (ret != 0) {
        log_error("PthreadBridge: pthread_create failed (error: %d)", ret);
    }

    return ret;
}

int PthreadBridge::join_wrapper(void* linuxThread, void** retval) {
    // Linux pthread_t is passed as a 32-bit value
    uint32_t tid = *(uint32_t*)linuxThread;

    EnsureMapLockInitialized();
    pthread_mutex_lock(&g_mapLock);

    auto it = g_threadMap.find(tid);
    if (it == g_threadMap.end()) {
        pthread_mutex_unlock(&g_mapLock);
        log_error("PthreadBridge: join called on unknown thread %u", tid);
        return -1;
    }

    pthread_t wthread = it->second;
    pthread_mutex_unlock(&g_mapLock);

    log_debug("PthreadBridge: Joining thread linux_tid=%u", tid);
    int ret = pthread_join(wthread, retval);

    // Remove from map after join
    pthread_mutex_lock(&g_mapLock);
    g_threadMap.erase(tid);
    pthread_mutex_unlock(&g_mapLock);

    return ret;
}

int PthreadBridge::detach_wrapper(void* linuxThread) {
    uint32_t tid = *(uint32_t*)linuxThread;

    EnsureMapLockInitialized();
    pthread_mutex_lock(&g_mapLock);

    auto it = g_threadMap.find(tid);
    if (it == g_threadMap.end()) {
        pthread_mutex_unlock(&g_mapLock);
        log_warn("PthreadBridge: detach called on unknown thread %u", tid);
        return 0;
    }

    pthread_t wthread = it->second;
    g_threadMap.erase(it);  // Remove from map since it's detached
    pthread_mutex_unlock(&g_mapLock);

    log_debug("PthreadBridge: Detaching thread linux_tid=%u", tid);
    return pthread_detach(wthread);
}

// ============================================================
// Condition Variable Wrappers
// ============================================================

int PthreadBridge::cond_init_wrapper(void* cond, const void* attr) {
    log_debug("PthreadBridge: cond_init %p", cond);
    GetOrCreateCond(cond);
    return 0;
}

int PthreadBridge::cond_wait_wrapper(void* cond, void* mutex) {
    pthread_cond_t* winCond = GetOrCreateCond(cond);
    pthread_mutex_t* winMutex = GetOrCreateMutex(mutex);

    log_debug(">>> cond_wait: cond=%p mutex=%p", cond, mutex);
    return pthread_cond_wait(winCond, winMutex);
}

int PthreadBridge::cond_timedwait_wrapper(void* cond, void* mutex, const struct timespec* abstime) {
    pthread_cond_t* winCond = GetOrCreateCond(cond);
    pthread_mutex_t* winMutex = GetOrCreateMutex(mutex);

    log_debug(">>> cond_timedwait: cond=%p mutex=%p", cond, mutex);
    return pthread_cond_timedwait(winCond, winMutex, abstime);
}

int PthreadBridge::cond_signal_wrapper(void* cond) {
    pthread_cond_t* winCond = GetOrCreateCond(cond);
    log_debug(">>> cond_signal: %p", cond);
    return pthread_cond_signal(winCond);
}

int PthreadBridge::cond_broadcast_wrapper(void* cond) {
    pthread_cond_t* winCond = GetOrCreateCond(cond);
    log_debug(">>> cond_broadcast: %p", cond);
    return pthread_cond_broadcast(winCond);
}

int PthreadBridge::cond_destroy_wrapper(void* cond) {
    EnsureMapLockInitialized();
    pthread_mutex_lock(&g_mapLock);

    auto it = g_condMap.find(cond);
    if (it != g_condMap.end()) {
        pthread_cond_destroy(it->second);
        delete it->second;
        g_condMap.erase(it);
        log_debug("PthreadBridge: Destroyed cond %p", cond);
    }

    pthread_mutex_unlock(&g_mapLock);
    return 0;
}

// ============================================================
// Cleanup - Call at program exit
// ============================================================

void PthreadBridge::Cleanup() {
    EnsureMapLockInitialized();
    pthread_mutex_lock(&g_mapLock);

    // Clean up mutexes
    for (auto& pair : g_mutexMap) {
        pthread_mutex_destroy(pair.second);
        delete pair.second;
    }
    g_mutexMap.clear();

    // Clean up condition variables
    for (auto& pair : g_condMap) {
        pthread_cond_destroy(pair.second);
        delete pair.second;
    }
    g_condMap.clear();

    // Thread map doesn't need cleanup (pthread_t handles are owned by pthreads lib)
    g_threadMap.clear();

    pthread_mutex_unlock(&g_mapLock);

    log_info("PthreadBridge: Cleanup complete");
}
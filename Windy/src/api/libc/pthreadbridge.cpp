#include "PthreadBridge.h"
#include <cstring>
#include <iostream>

#include "../src/core/log.h"

// --- Helper: Lazy Initialization with Recursive Attribute ---

static void EnsureMutexInitialized(pthread_mutex_t* wm) {
    if (*wm == NULL) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(wm, &attr);
        pthread_mutexattr_destroy(&attr);
        log_debug("PthreadBridge: Initialized recursive mutex at %p", wm);
    }
}

static void EnsureCondInitialized(pthread_cond_t* wc) {
    if (*wc == NULL) {
        pthread_cond_init(wc, NULL);
    }
}

// --- Mutex Wrappers ---

int PthreadBridge::mutex_init_wrapper(void* mutex, const void* attr) {
    if (attr == NULL) {
        pthread_mutexattr_t recursiveAttr;
        pthread_mutexattr_init(&recursiveAttr);
        pthread_mutexattr_settype(&recursiveAttr, PTHREAD_MUTEX_RECURSIVE);
        int ret = pthread_mutex_init((pthread_mutex_t*)mutex, &recursiveAttr);
        pthread_mutexattr_destroy(&recursiveAttr);
        return ret;
    }
    return pthread_mutex_init((pthread_mutex_t*)mutex, (const pthread_mutexattr_t*)attr);
}

int PthreadBridge::mutex_lock_wrapper(void* mutex) {
    pthread_mutex_t* wm = (pthread_mutex_t*)mutex;
    EnsureMutexInitialized(wm);

    log_info(">>> mutex_lock: %p", mutex);

    int ret = pthread_mutex_lock(wm);

    if (ret != 0) {
        log_error("PthreadBridge: mutex_lock failed for %p (error: %d)", mutex, ret);
    }

    return ret;
}

int PthreadBridge::mutex_trylock_wrapper(void* mutex) {
    pthread_mutex_t* wm = (pthread_mutex_t*)mutex;
    EnsureMutexInitialized(wm);
    return pthread_mutex_trylock(wm);
}

int PthreadBridge::mutex_unlock_wrapper(void* mutex) {
    pthread_mutex_t* wm = (pthread_mutex_t*)mutex;
    if (*wm == NULL) return 0;
    return pthread_mutex_unlock(wm);
}

int PthreadBridge::mutex_destroy_wrapper(void* mutex) {
    return pthread_mutex_destroy((pthread_mutex_t*)mutex);
}

// --- Thread Wrappers ---

int PthreadBridge::create_wrapper(void* thread, const void* attr, void* (*start_routine)(void*), void* arg) {
    pthread_t wthread;
    int ret = pthread_create(&wthread, (const pthread_attr_t*)attr, start_routine, arg);

    if (ret == 0 && thread) {
        memcpy(thread, &wthread, sizeof(uint32_t));
    }
    return ret;
}

int PthreadBridge::join_wrapper(pthread_t thread, void** retval) {
    return pthread_join(thread, retval);
}

int PthreadBridge::detach_wrapper(pthread_t thread) {
    return pthread_detach(thread);
}

// --- Cond Wrappers ---

int PthreadBridge::cond_init_wrapper(void* cond, const void* attr) {
    return pthread_cond_init((pthread_cond_t*)cond, (const pthread_condattr_t*)attr);
}

int PthreadBridge::cond_wait_wrapper(void* cond, void* mutex) {
    log_info(">>> cond_wait: cond=%p mutex=%p", cond, mutex);
    pthread_cond_t* wc = (pthread_cond_t*)cond;
    pthread_mutex_t* wm = (pthread_mutex_t*)mutex;
    EnsureCondInitialized(wc);
    EnsureMutexInitialized(wm);
    return pthread_cond_wait(wc, wm);
}

int PthreadBridge::cond_signal_wrapper(void* cond) {
    pthread_cond_t* wc = (pthread_cond_t*)cond;
    EnsureCondInitialized(wc);
    return pthread_cond_signal(wc);
}

int PthreadBridge::cond_broadcast_wrapper(void* cond) {
    pthread_cond_t* wc = (pthread_cond_t*)cond;
    EnsureCondInitialized(wc);
    return pthread_cond_broadcast(wc);
}

int PthreadBridge::cond_destroy_wrapper(void* cond) {
    return pthread_cond_destroy((pthread_cond_t*)cond);
}
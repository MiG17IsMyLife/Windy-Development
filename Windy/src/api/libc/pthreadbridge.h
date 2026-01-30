#pragma once

#ifndef HAVE_STRUCT_TIMESPEC
#define HAVE_STRUCT_TIMESPEC
#endif
#include <pthread.h>
#include <stdint.h>

// ============================================================
// PthreadBridge - Linux to Windows pthread compatibility layer
// ============================================================

class PthreadBridge {
public:
    // Mutex wrappers - take Linux mutex address, map to Windows mutex
    static int mutex_init_wrapper(void* mutex, const void* attr);
    static int mutex_lock_wrapper(void* mutex);
    static int mutex_trylock_wrapper(void* mutex);
    static int mutex_unlock_wrapper(void* mutex);
    static int mutex_destroy_wrapper(void* mutex);

    // Thread wrappers - handle Linux/Windows pthread_t differences
    static int create_wrapper(void* thread, const void* attr, void* (*start_routine)(void*), void* arg);
    static int join_wrapper(void* linuxThread, void** retval);
    static int detach_wrapper(void* linuxThread);

    // Condition variable wrappers
    static int cond_init_wrapper(void* cond, const void* attr);
    static int cond_wait_wrapper(void* cond, void* mutex);
    static int cond_timedwait_wrapper(void* cond, void* mutex, const struct timespec* abstime);
    static int cond_signal_wrapper(void* cond);
    static int cond_broadcast_wrapper(void* cond);
    static int cond_destroy_wrapper(void* cond);

    // Cleanup
    static void Cleanup();
};
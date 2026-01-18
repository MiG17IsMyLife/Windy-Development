#pragma once

#ifndef HAVE_STRUCT_TIMESPEC
#define HAVE_STRUCT_TIMESPEC
#endif
#include <pthread.h>

class PthreadBridge {
public:
    // Mutex
    static int mutex_init_wrapper(void* mutex, const void* attr);
    static int mutex_lock_wrapper(void* mutex);
    static int mutex_trylock_wrapper(void* mutex);
    static int mutex_unlock_wrapper(void* mutex);
    static int mutex_destroy_wrapper(void* mutex);

    // Thread
    static int create_wrapper(void* thread, const void* attr, void* (*start_routine)(void*), void* arg);
    static int join_wrapper(pthread_t thread, void** retval);
    static int detach_wrapper(pthread_t thread);

    // Others
    static int cond_init_wrapper(void* cond, const void* attr);
    static int cond_wait_wrapper(void* cond, void* mutex);
    static int cond_signal_wrapper(void* cond);
    static int cond_broadcast_wrapper(void* cond);
    static int cond_destroy_wrapper(void* cond);
};
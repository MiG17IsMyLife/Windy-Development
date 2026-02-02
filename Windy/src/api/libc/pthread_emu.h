#pragma once

// ============================================================
// pthread_emu.h - Linux pthread API Emulation
// ============================================================

#ifndef PTHREAD_EMU_H
#define PTHREAD_EMU_H

#include <stdint.h>

// timespec forward declaration
struct timespec;

// ============================================================
// Main PthreadEmu Class
// ============================================================

class PthreadEmu {
public:
    // Init/Shutdown
    static void Initialize();
    static void Shutdown();

    // --------------------------------------------------------
    // Thread API
    // --------------------------------------------------------
    static int pthread_create(void* thread, const void* attr,
        void* (*start_routine)(void*), void* arg);
    static int pthread_join(uint32_t thread, void** retval);
    static int pthread_detach(uint32_t thread);
    static void pthread_exit(void* retval);
    static uint32_t pthread_self();
    static int pthread_equal(uint32_t t1, uint32_t t2);

    // Thread cancel (limited support)
    static int pthread_cancel(uint32_t thread);
    static int pthread_setcancelstate(int state, int* oldstate);
    static int pthread_setcanceltype(int type, int* oldtype);
    static void pthread_testcancel();

    // Thread attributes
    static int pthread_attr_init(void* attr);
    static int pthread_attr_destroy(void* attr);
    static int pthread_attr_setdetachstate(void* attr, int detachstate);
    static int pthread_attr_getdetachstate(const void* attr, int* detachstate);
    static int pthread_attr_setstacksize(void* attr, size_t stacksize);
    static int pthread_attr_getstacksize(const void* attr, size_t* stacksize);

    // --------------------------------------------------------
    // Mutex API
    // --------------------------------------------------------
    static int pthread_mutex_init(void* mutex, const void* attr);
    static int pthread_mutex_destroy(void* mutex);
    static int pthread_mutex_lock(void* mutex);
    static int pthread_mutex_trylock(void* mutex);
    static int pthread_mutex_timedlock(void* mutex, const struct timespec* abstime);
    static int pthread_mutex_unlock(void* mutex);

    // Mutex attributes
    static int pthread_mutexattr_init(void* attr);
    static int pthread_mutexattr_destroy(void* attr);
    static int pthread_mutexattr_settype(void* attr, int type);
    static int pthread_mutexattr_gettype(const void* attr, int* type);
    static int pthread_mutexattr_setpshared(void* attr, int pshared);
    static int pthread_mutexattr_getpshared(const void* attr, int* pshared);

    // --------------------------------------------------------
    // Condition Variable API
    // --------------------------------------------------------
    static int pthread_cond_init(void* cond, const void* attr);
    static int pthread_cond_destroy(void* cond);
    static int pthread_cond_wait(void* cond, void* mutex);
    static int pthread_cond_timedwait(void* cond, void* mutex,
        const struct timespec* abstime);
    static int pthread_cond_signal(void* cond);
    static int pthread_cond_broadcast(void* cond);

    // Cond attributes
    static int pthread_condattr_init(void* attr);
    static int pthread_condattr_destroy(void* attr);
    static int pthread_condattr_setpshared(void* attr, int pshared);
    static int pthread_condattr_getpshared(const void* attr, int* pshared);
    static int pthread_condattr_setclock(void* attr, int clock_id);
    static int pthread_condattr_getclock(const void* attr, int* clock_id);

    // --------------------------------------------------------
    // Read-Write Lock API
    // --------------------------------------------------------
    static int pthread_rwlock_init(void* rwlock, const void* attr);
    static int pthread_rwlock_destroy(void* rwlock);
    static int pthread_rwlock_rdlock(void* rwlock);
    static int pthread_rwlock_tryrdlock(void* rwlock);
    static int pthread_rwlock_timedrdlock(void* rwlock, const struct timespec* abstime);
    static int pthread_rwlock_wrlock(void* rwlock);
    static int pthread_rwlock_trywrlock(void* rwlock);
    static int pthread_rwlock_timedwrlock(void* rwlock, const struct timespec* abstime);
    static int pthread_rwlock_unlock(void* rwlock);

    // Rwlock attributes
    static int pthread_rwlockattr_init(void* attr);
    static int pthread_rwlockattr_destroy(void* attr);
    static int pthread_rwlockattr_setpshared(void* attr, int pshared);
    static int pthread_rwlockattr_getpshared(const void* attr, int* pshared);

    // --------------------------------------------------------
    // Barrier API
    // --------------------------------------------------------
    static int pthread_barrier_init(void* barrier, const void* attr,
        unsigned int count);
    static int pthread_barrier_destroy(void* barrier);
    static int pthread_barrier_wait(void* barrier);

    // Barrier attributes
    static int pthread_barrierattr_init(void* attr);
    static int pthread_barrierattr_destroy(void* attr);
    static int pthread_barrierattr_setpshared(void* attr, int pshared);
    static int pthread_barrierattr_getpshared(const void* attr, int* pshared);

    // --------------------------------------------------------
    // Spinlock API
    // --------------------------------------------------------
    static int pthread_spin_init(void* lock, int pshared);
    static int pthread_spin_destroy(void* lock);
    static int pthread_spin_lock(void* lock);
    static int pthread_spin_trylock(void* lock);
    static int pthread_spin_unlock(void* lock);

    // --------------------------------------------------------
    // Once API
    // --------------------------------------------------------
    static int pthread_once(void* once_control, void (*init_routine)(void));

    // --------------------------------------------------------
    // Thread-Local Storage (TLS) API
    // --------------------------------------------------------
    static int pthread_key_create(void* key, void (*destructor)(void*));
    static int pthread_key_delete(uint32_t key);
    static int pthread_setspecific(uint32_t key, const void* value);
    static void* pthread_getspecific(uint32_t key);

    // --------------------------------------------------------
    // Scheduling API (limited support)
    // --------------------------------------------------------
    static int pthread_setschedparam(uint32_t thread, int policy,
        const void* param);
    static int pthread_getschedparam(uint32_t thread, int* policy,
        void* param);
    static int sched_yield();

    // --------------------------------------------------------
    // Misc
    // --------------------------------------------------------
    static int pthread_getconcurrency();
    static int pthread_setconcurrency(int new_level);
};

// ============================================================
// Semaphore API (C functions)
// ============================================================

extern "C" {
    int emu_sem_init(void* sem, int pshared, unsigned int value);
    int emu_sem_destroy(void* sem);
    int emu_sem_wait(void* sem);
    int emu_sem_trywait(void* sem);
    int emu_sem_timedwait(void* sem, const struct timespec* abs_timeout);
    int emu_sem_post(void* sem);
    int emu_sem_getvalue(void* sem, int* sval);

    // Cleanup function for semaphores (called by PthreadEmu::Shutdown)
    void SemaphoreCleanup();
}

// ============================================================
// C API Exports for DLL Hooking but, not really used
// ============================================================

#ifdef PTHREAD_EMU_EXPORT_C_API
extern "C" {
    int emu_pthread_create(void* thread, const void* attr,
        void* (*start_routine)(void*), void* arg);
    int emu_pthread_join(uint32_t thread, void** retval);
    // ... etc
}
#endif

#endif // PTHREAD_EMU_H
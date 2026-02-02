// ============================================================
// pthread_emu.cpp - Init/Shutdown and C API Exports
// ============================================================

#include "pthread_emu.h"
#include "pthread_internal.h"

// External declaration for TLS destructor support
extern void CallTlsDestructors();

// ============================================================
// Global State
// ============================================================

static volatile bool s_emu_initialized = false;
static CRITICAL_SECTION s_init_lock;
static volatile bool s_init_lock_ready = false;

// One-time initialization for the init lock itself
static void EnsureInitLock() {
    if (!s_init_lock_ready) {
        // This is a potential race, but only at very first initialization
        // In practice, Initialize() should be called from main thread first
        InitializeCriticalSection(&s_init_lock);
        s_init_lock_ready = true;
    }
}

// ============================================================
// Initialize / Shutdown
// ============================================================

void PthreadEmu::Initialize() {
    EnsureInitLock();

    EnterCriticalSection(&s_init_lock);

    if (s_emu_initialized) {
        LeaveCriticalSection(&s_init_lock);
        return;
    }

    // Initialize the mapper (handles all object tracking)
    PthreadMapper::Initialize();

    s_emu_initialized = true;

    LeaveCriticalSection(&s_init_lock);
}

void PthreadEmu::Shutdown() {
    EnsureInitLock();

    EnterCriticalSection(&s_init_lock);

    if (!s_emu_initialized) {
        LeaveCriticalSection(&s_init_lock);
        return;
    }

    // Call TLS destructors for main thread
    CallTlsDestructors();

    // Cleanup semaphores
    SemaphoreCleanup();

    // Shutdown the mapper (cleans up all objects)
    PthreadMapper::Shutdown();

    s_emu_initialized = false;

    LeaveCriticalSection(&s_init_lock);
}

// ============================================================
// C API Exports (optional, for direct hooking)
// ============================================================

#ifdef PTHREAD_EMU_EXPORT_C_API

extern "C" {

    // --- Initialization ---
    void pthread_emu_init() {
        PthreadEmu::Initialize();
    }

    void pthread_emu_shutdown() {
        PthreadEmu::Shutdown();
    }

    // --- Thread Functions ---
    int emu_pthread_create(void* thread, const void* attr,
        void* (*start_routine)(void*), void* arg) {
        return PthreadEmu::pthread_create(thread, attr, start_routine, arg);
    }

    int emu_pthread_join(uint32_t thread, void** retval) {
        return PthreadEmu::pthread_join(thread, retval);
    }

    int emu_pthread_detach(uint32_t thread) {
        return PthreadEmu::pthread_detach(thread);
    }

    void emu_pthread_exit(void* retval) {
        PthreadEmu::pthread_exit(retval);
    }

    uint32_t emu_pthread_self() {
        return PthreadEmu::pthread_self();
    }

    int emu_pthread_equal(uint32_t t1, uint32_t t2) {
        return PthreadEmu::pthread_equal(t1, t2);
    }

    // --- Mutex Functions ---
    int emu_pthread_mutex_init(void* mutex, const void* attr) {
        return PthreadEmu::pthread_mutex_init(mutex, attr);
    }

    int emu_pthread_mutex_destroy(void* mutex) {
        return PthreadEmu::pthread_mutex_destroy(mutex);
    }

    int emu_pthread_mutex_lock(void* mutex) {
        return PthreadEmu::pthread_mutex_lock(mutex);
    }

    int emu_pthread_mutex_trylock(void* mutex) {
        return PthreadEmu::pthread_mutex_trylock(mutex);
    }

    int emu_pthread_mutex_timedlock(void* mutex, const struct timespec* abstime) {
        return PthreadEmu::pthread_mutex_timedlock(mutex, abstime);
    }

    int emu_pthread_mutex_unlock(void* mutex) {
        return PthreadEmu::pthread_mutex_unlock(mutex);
    }

    // --- Mutex Attribute Functions ---
    int emu_pthread_mutexattr_init(void* attr) {
        return PthreadEmu::pthread_mutexattr_init(attr);
    }

    int emu_pthread_mutexattr_destroy(void* attr) {
        return PthreadEmu::pthread_mutexattr_destroy(attr);
    }

    int emu_pthread_mutexattr_settype(void* attr, int type) {
        return PthreadEmu::pthread_mutexattr_settype(attr, type);
    }

    int emu_pthread_mutexattr_gettype(const void* attr, int* type) {
        return PthreadEmu::pthread_mutexattr_gettype(attr, type);
    }

    // --- Condition Variable Functions ---
    int emu_pthread_cond_init(void* cond, const void* attr) {
        return PthreadEmu::pthread_cond_init(cond, attr);
    }

    int emu_pthread_cond_destroy(void* cond) {
        return PthreadEmu::pthread_cond_destroy(cond);
    }

    int emu_pthread_cond_wait(void* cond, void* mutex) {
        return PthreadEmu::pthread_cond_wait(cond, mutex);
    }

    int emu_pthread_cond_timedwait(void* cond, void* mutex, const struct timespec* abstime) {
        return PthreadEmu::pthread_cond_timedwait(cond, mutex, abstime);
    }

    int emu_pthread_cond_signal(void* cond) {
        return PthreadEmu::pthread_cond_signal(cond);
    }

    int emu_pthread_cond_broadcast(void* cond) {
        return PthreadEmu::pthread_cond_broadcast(cond);
    }

    // --- Read-Write Lock Functions ---
    int emu_pthread_rwlock_init(void* rwlock, const void* attr) {
        return PthreadEmu::pthread_rwlock_init(rwlock, attr);
    }

    int emu_pthread_rwlock_destroy(void* rwlock) {
        return PthreadEmu::pthread_rwlock_destroy(rwlock);
    }

    int emu_pthread_rwlock_rdlock(void* rwlock) {
        return PthreadEmu::pthread_rwlock_rdlock(rwlock);
    }

    int emu_pthread_rwlock_tryrdlock(void* rwlock) {
        return PthreadEmu::pthread_rwlock_tryrdlock(rwlock);
    }

    int emu_pthread_rwlock_wrlock(void* rwlock) {
        return PthreadEmu::pthread_rwlock_wrlock(rwlock);
    }

    int emu_pthread_rwlock_trywrlock(void* rwlock) {
        return PthreadEmu::pthread_rwlock_trywrlock(rwlock);
    }

    int emu_pthread_rwlock_unlock(void* rwlock) {
        return PthreadEmu::pthread_rwlock_unlock(rwlock);
    }

    // --- Barrier Functions ---
    int emu_pthread_barrier_init(void* barrier, const void* attr, unsigned int count) {
        return PthreadEmu::pthread_barrier_init(barrier, attr, count);
    }

    int emu_pthread_barrier_destroy(void* barrier) {
        return PthreadEmu::pthread_barrier_destroy(barrier);
    }

    int emu_pthread_barrier_wait(void* barrier) {
        return PthreadEmu::pthread_barrier_wait(barrier);
    }

    // --- Spinlock Functions ---
    int emu_pthread_spin_init(void* lock, int pshared) {
        return PthreadEmu::pthread_spin_init(lock, pshared);
    }

    int emu_pthread_spin_destroy(void* lock) {
        return PthreadEmu::pthread_spin_destroy(lock);
    }

    int emu_pthread_spin_lock(void* lock) {
        return PthreadEmu::pthread_spin_lock(lock);
    }

    int emu_pthread_spin_trylock(void* lock) {
        return PthreadEmu::pthread_spin_trylock(lock);
    }

    int emu_pthread_spin_unlock(void* lock) {
        return PthreadEmu::pthread_spin_unlock(lock);
    }

    // --- Once Function ---
    int emu_pthread_once(void* once_control, void (*init_routine)(void)) {
        return PthreadEmu::pthread_once(once_control, init_routine);
    }

    // --- TLS Functions ---
    int emu_pthread_key_create(void* key, void (*destructor)(void*)) {
        return PthreadEmu::pthread_key_create(key, destructor);
    }

    int emu_pthread_key_delete(uint32_t key) {
        return PthreadEmu::pthread_key_delete(key);
    }

    int emu_pthread_setspecific(uint32_t key, const void* value) {
        return PthreadEmu::pthread_setspecific(key, value);
    }

    void* emu_pthread_getspecific(uint32_t key) {
        return PthreadEmu::pthread_getspecific(key);
    }

    // --- Scheduling ---
    int emu_sched_yield() {
        return PthreadEmu::sched_yield();
    }

} // extern "C"

#endif // PTHREAD_EMU_EXPORT_C_API
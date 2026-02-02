// ============================================================
// pthread_spin.cpp - Spinlock Emulation
// ============================================================

#include "pthread_emu.h"
#include "pthread_internal.h"

// ============================================================
// Spinlock Functions
// ============================================================

int PthreadEmu::pthread_spin_init(void* lock, int pshared) {
    if (!lock) return LINUX_EINVAL;
    
    // Only PRIVATE is supported (SHARED requires shared memory)
    if (pshared != LINUX_PTHREAD_PROCESS_PRIVATE) {
        return LINUX_EINVAL;
    }
    
    PthreadSpinInternal* s = PthreadMapper::GetOrCreateSpin(lock);
    if (!s) return LINUX_ENOMEM;
    
    // Initialize to unlocked state
    InterlockedExchange(&s->lock, 0);
    
    return 0;
}

int PthreadEmu::pthread_spin_destroy(void* lock) {
    if (!lock) return LINUX_EINVAL;
    
    PthreadSpinInternal* s = PthreadMapper::FindSpin(lock);
    if (!s) {
        // Already destroyed or never initialized
        return 0;
    }
    
    // Check if currently locked
    if (s->lock != 0) {
        return LINUX_EBUSY;
    }
    
    PthreadMapper::DestroySpin(lock);
    return 0;
}

int PthreadEmu::pthread_spin_lock(void* lock) {
    if (!lock) return LINUX_EINVAL;
    
    PthreadSpinInternal* s = PthreadMapper::GetOrCreateSpin(lock);
    if (!s) return LINUX_EINVAL;
    
    // Spin until we acquire the lock
    // Use test-and-test-and-set for better cache behavior
    int spin_count = 0;
    const int MAX_SPIN_BEFORE_YIELD = 1000;
    
    while (true) {
        // First, check if lock is available (read-only, cache-friendly)
        if (s->lock == 0) {
            // Try to acquire
            if (InterlockedCompareExchange(&s->lock, 1, 0) == 0) {
                // Got the lock
                return 0;
            }
        }
        
        // Lock is held by someone else
        spin_count++;
        
        if (spin_count < MAX_SPIN_BEFORE_YIELD) {
            // Use pause instruction to reduce power and improve performance
            YieldProcessor();
        } else {
            // After spinning for a while, yield to other threads
            // This helps prevent complete CPU starvation
            SwitchToThread();
            spin_count = 0;
        }
    }
}

int PthreadEmu::pthread_spin_trylock(void* lock) {
    if (!lock) return LINUX_EINVAL;
    
    PthreadSpinInternal* s = PthreadMapper::GetOrCreateSpin(lock);
    if (!s) return LINUX_EINVAL;
    
    // Single attempt to acquire
    if (InterlockedCompareExchange(&s->lock, 1, 0) != 0) {
        return LINUX_EBUSY;
    }
    
    return 0;
}

int PthreadEmu::pthread_spin_unlock(void* lock) {
    if (!lock) return LINUX_EINVAL;
    
    PthreadSpinInternal* s = PthreadMapper::FindSpin(lock);
    if (!s) return LINUX_EINVAL;
    
    // Release the lock
    // Use InterlockedExchange for proper memory ordering
    InterlockedExchange(&s->lock, 0);
    
    return 0;
}

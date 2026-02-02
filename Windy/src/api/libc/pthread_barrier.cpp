// ============================================================
// pthread_barrier.cpp - Barrier Emulation
// ============================================================

#include "pthread_emu.h"
#include "pthread_internal.h"

// ============================================================
// Barrier Attribute Functions
// ============================================================

int PthreadEmu::pthread_barrierattr_init(void* attr) {
    if (!attr) return LINUX_EINVAL;
    
    PthreadBarrierAttrInternal* a = new PthreadBarrierAttrInternal;
    a->pshared = LINUX_PTHREAD_PROCESS_PRIVATE;
    
    *(void**)attr = a;
    return 0;
}

int PthreadEmu::pthread_barrierattr_destroy(void* attr) {
    if (!attr) return LINUX_EINVAL;
    
    PthreadBarrierAttrInternal* a = *(PthreadBarrierAttrInternal**)attr;
    if (a) {
        delete a;
        *(void**)attr = nullptr;
    }
    return 0;
}

int PthreadEmu::pthread_barrierattr_setpshared(void* attr, int pshared) {
    if (!attr) return LINUX_EINVAL;
    
    PthreadBarrierAttrInternal* a = *(PthreadBarrierAttrInternal**)attr;
    if (!a) return LINUX_EINVAL;
    
    // Only PRIVATE is supported
    if (pshared != LINUX_PTHREAD_PROCESS_PRIVATE) {
        return LINUX_EINVAL;
    }
    
    a->pshared = pshared;
    return 0;
}

int PthreadEmu::pthread_barrierattr_getpshared(const void* attr, int* pshared) {
    if (!attr || !pshared) return LINUX_EINVAL;
    
    PthreadBarrierAttrInternal* a = *(PthreadBarrierAttrInternal**)attr;
    if (!a) return LINUX_EINVAL;
    
    *pshared = a->pshared;
    return 0;
}

// ============================================================
// Barrier Functions
// ============================================================

int PthreadEmu::pthread_barrier_init(void* barrier, const void* attr, unsigned int count) {
    if (!barrier) return LINUX_EINVAL;
    if (count == 0) return LINUX_EINVAL;
    
    PthreadBarrierInternal* b = PthreadMapper::GetOrCreateBarrier(barrier, count);
    if (!b) return LINUX_ENOMEM;
    
    // Reset state in case of reinitialization
    EnterCriticalSection(&b->cs);
    b->threshold = count;
    b->count = 0;
    b->generation = 0;
    LeaveCriticalSection(&b->cs);
    
    return 0;
}

int PthreadEmu::pthread_barrier_destroy(void* barrier) {
    if (!barrier) return LINUX_EINVAL;
    
    PthreadBarrierInternal* b = PthreadMapper::FindBarrier(barrier);
    if (!b) {
        // Already destroyed or never initialized
        return 0;
    }
    
    // Check if threads are waiting
    EnterCriticalSection(&b->cs);
    if (b->count > 0) {
        LeaveCriticalSection(&b->cs);
        return LINUX_EBUSY;
    }
    LeaveCriticalSection(&b->cs);
    
    PthreadMapper::DestroyBarrier(barrier);
    return 0;
}

int PthreadEmu::pthread_barrier_wait(void* barrier) {
    if (!barrier) return LINUX_EINVAL;
    
    PthreadBarrierInternal* b = PthreadMapper::FindBarrier(barrier);
    if (!b) {
        // Try to auto-initialize (shouldn't happen in well-formed code)
        b = PthreadMapper::GetOrCreateBarrier(barrier, 1);
        if (!b) return LINUX_EINVAL;
    }
    
    EnterCriticalSection(&b->cs);
    
    // Remember current generation
    unsigned int my_generation = b->generation;
    
    // Increment waiting count
    b->count++;
    
    if (b->count >= b->threshold) {
        // We are the last thread to arrive - release everyone
        b->generation++;  // Move to next generation
        b->count = 0;     // Reset count for next use
        
        // Wake all waiting threads
        WakeAllConditionVariable(&b->cv);
        
        LeaveCriticalSection(&b->cs);
        
        // Return special value to indicate this thread was the "serial" thread
        return LINUX_PTHREAD_BARRIER_SERIAL_THREAD;
    }
    
    // Not the last thread - wait for others
    // Use generation to handle spurious wakeups correctly
    while (my_generation == b->generation) {
        SleepConditionVariableCS(&b->cv, &b->cs, INFINITE);
    }
    
    LeaveCriticalSection(&b->cs);
    return 0;
}

// ============================================================
// pthread_tls.cpp - Thread-Local Storage Emulation
// ============================================================

#include "pthread_emu.h"
#include "pthread_internal.h"

// ============================================================
// TLS Key Management
// ============================================================

#define MAX_TLS_KEYS 1024

struct TlsKeyEntry {
    DWORD win_tls_index;
    void (*destructor)(void*);
    bool in_use;
};

static TlsKeyEntry s_tls_keys[MAX_TLS_KEYS];
static CRITICAL_SECTION s_tls_lock;
static bool s_tls_initialized = false;

static void InitTlsSystem() {
    if (!s_tls_initialized) {
        InitializeCriticalSection(&s_tls_lock);
        memset(s_tls_keys, 0, sizeof(s_tls_keys));
        s_tls_initialized = true;
    }
}

// ============================================================
// TLS Destructor Support
// ============================================================

// Call destructors for thread-local values when thread exits
// This needs to be called from thread exit path
void CallTlsDestructors() {
    if (!s_tls_initialized) return;
    
    // POSIX allows up to PTHREAD_DESTRUCTOR_ITERATIONS attempts
    const int MAX_ITERATIONS = 4;
    
    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        bool any_non_null = false;
        
        EnterCriticalSection(&s_tls_lock);
        
        for (uint32_t key = 0; key < MAX_TLS_KEYS; key++) {
            if (s_tls_keys[key].in_use && s_tls_keys[key].destructor) {
                void* value = TlsGetValue(s_tls_keys[key].win_tls_index);
                if (value) {
                    // Clear value before calling destructor
                    TlsSetValue(s_tls_keys[key].win_tls_index, NULL);
                    
                    // Call destructor outside the lock
                    void (*dtor)(void*) = s_tls_keys[key].destructor;
                    LeaveCriticalSection(&s_tls_lock);
                    
                    dtor(value);
                    any_non_null = true;
                    
                    EnterCriticalSection(&s_tls_lock);
                }
            }
        }
        
        LeaveCriticalSection(&s_tls_lock);
        
        if (!any_non_null) break;
    }
}

// ============================================================
// TLS Functions
// ============================================================

int PthreadEmu::pthread_key_create(void* key, void (*destructor)(void*)) {
    if (!key) return LINUX_EINVAL;
    
    InitTlsSystem();
    
    EnterCriticalSection(&s_tls_lock);
    
    // Find free slot
    uint32_t found_key = (uint32_t)-1;
    for (uint32_t i = 0; i < MAX_TLS_KEYS; i++) {
        if (!s_tls_keys[i].in_use) {
            found_key = i;
            break;
        }
    }
    
    if (found_key == (uint32_t)-1) {
        LeaveCriticalSection(&s_tls_lock);
        return LINUX_EAGAIN;  // No more keys available
    }
    
    // Allocate Windows TLS slot
    DWORD win_index = TlsAlloc();
    if (win_index == TLS_OUT_OF_INDEXES) {
        LeaveCriticalSection(&s_tls_lock);
        return LINUX_EAGAIN;
    }
    
    s_tls_keys[found_key].win_tls_index = win_index;
    s_tls_keys[found_key].destructor = destructor;
    s_tls_keys[found_key].in_use = true;
    
    LeaveCriticalSection(&s_tls_lock);
    
    // Return key to caller
    *(uint32_t*)key = found_key;
    return 0;
}

int PthreadEmu::pthread_key_delete(uint32_t key) {
    if (key >= MAX_TLS_KEYS) return LINUX_EINVAL;
    
    InitTlsSystem();
    
    EnterCriticalSection(&s_tls_lock);
    
    if (!s_tls_keys[key].in_use) {
        LeaveCriticalSection(&s_tls_lock);
        return LINUX_EINVAL;
    }
    
    // Free Windows TLS slot
    TlsFree(s_tls_keys[key].win_tls_index);
    
    s_tls_keys[key].in_use = false;
    s_tls_keys[key].destructor = NULL;
    
    LeaveCriticalSection(&s_tls_lock);
    return 0;
}

int PthreadEmu::pthread_setspecific(uint32_t key, const void* value) {
    if (key >= MAX_TLS_KEYS) return LINUX_EINVAL;
    
    InitTlsSystem();
    
    EnterCriticalSection(&s_tls_lock);
    
    if (!s_tls_keys[key].in_use) {
        LeaveCriticalSection(&s_tls_lock);
        return LINUX_EINVAL;
    }
    
    DWORD win_index = s_tls_keys[key].win_tls_index;
    
    LeaveCriticalSection(&s_tls_lock);
    
    if (!TlsSetValue(win_index, (LPVOID)value)) {
        return LINUX_EINVAL;
    }
    
    return 0;
}

void* PthreadEmu::pthread_getspecific(uint32_t key) {
    if (key >= MAX_TLS_KEYS) return NULL;
    
    InitTlsSystem();
    
    EnterCriticalSection(&s_tls_lock);
    
    if (!s_tls_keys[key].in_use) {
        LeaveCriticalSection(&s_tls_lock);
        return NULL;
    }
    
    DWORD win_index = s_tls_keys[key].win_tls_index;
    
    LeaveCriticalSection(&s_tls_lock);
    
    return TlsGetValue(win_index);
}

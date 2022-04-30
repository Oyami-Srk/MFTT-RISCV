#include <common/types.h>
#include <lib/stdlib.h>
#include <lib/sys/spinlock.h>
#include <riscv.h>
#include <trap.h>

static bool spinlock_holding(spinlock_t *pLock) {
    bool r;
    r = (pLock->lock) && (pLock->cpu == cpuid());
    return r;
}

void spinlock_init(spinlock_t *pLock) {
    pLock->lock = 0;
    pLock->cpu  = 0;
}

void spinlock_acquire(spinlock_t *pLock) {
    trap_push_off();
    assert(!spinlock_holding(pLock), "Lock is already be acquired.");
    while (__sync_lock_test_and_set(&pLock->lock, 1) != 0)
        ;
    __sync_synchronize();
    pLock->cpu = cpuid();
}

void spinlock_release(spinlock_t *pLock) {
    assert(pLock, "Lock cannot be null");
    assert(spinlock_holding(pLock), "Current CPU Not holding the lock.");
    pLock->cpu = 0;
    __sync_synchronize();
    __sync_lock_release(&pLock->lock);
    trap_pop_off();
}

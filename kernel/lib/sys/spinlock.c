#include <common/types.h>
#include <lib/stdlib.h>
#include <lib/sys/spinlock.h>
#include <trap.h>

static bool holding(spinlock_t *pLock) { return (pLock->lock); }

void spinlock_init(spinlock_t *pLock) { pLock->lock = 0; }

void spinlock_acquire(spinlock_t *pLock) {
    assert(!holding(pLock), "Lock is already be acquired.");
    trap_push_off();
    while (__sync_lock_test_and_set(&pLock->lock, 1) != 0)
        ;
    __sync_synchronize();
}

void spinlock_release(spinlock_t *pLock) {
    assert(pLock && holding(pLock), "Current CPU Not holding the lock.");
    __sync_synchronize();
    __sync_lock_release(&pLock->lock);
    trap_pop_off();
}

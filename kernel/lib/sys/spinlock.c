#include <common/types.h>
#include <lib/stdlib.h>
#include <lib/sys/spinlock.h>
#include <riscv.h>
#include <smp_barrier.h>
#include <trap.h>

static bool spinlock_holding(spinlock_t *pLock) {
    return (READ_ONCE(pLock->lock) != 0);
}

void spinlock_init(spinlock_t *pLock) {
    pLock->lock = 0;
    pLock->cpu  = 0;
}

static inline int spinlock_try_lock(spinlock_t *pLock) {
    int tmp = 1, busy;
    asm volatile("amoswap.w %0, %2, %1\n\t"
                 "fence r, rw"
                 : "=r"(busy), "+A"(pLock->lock)
                 : "r"(tmp)
                 : "memory");
    return !busy;
}

void spinlock_acquire(spinlock_t *pLock) {
    trap_push_off();
    /*
    assert(!spinlock_holding(pLock), "Lock is already be acquired.");
    while (__sync_lock_test_and_set(&pLock->lock, 1) != 0)
        ;
    __sync_synchronize();
    pLock->cpu = cpuid();
     */
    while (1) {
        if (spinlock_holding(pLock))
            continue;
        if (spinlock_try_lock(pLock))
            break;
    }
}

void spinlock_release(spinlock_t *pLock) {
    /*
    assert(pLock, "Lock cannot be null");
    assert(spinlock_holding(pLock), "Current CPU Not holding the lock.");
    pLock->cpu = 0;
    __sync_synchronize();
    __sync_lock_release(&pLock->lock);
     */
    RISCV_FENCE(rw, w);
    WRITE_ONCE(pLock->lock, 0);
    trap_pop_off();
}

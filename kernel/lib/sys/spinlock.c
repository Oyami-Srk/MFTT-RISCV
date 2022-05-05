#include <common/types.h>
#include <lib/stdlib.h>
#include <lib/sys/spinlock.h>
#include <riscv.h>
#include <smp_barrier.h>
#include <trap.h>

// #define LINUX_WAY

static bool spinlock_holding(spinlock_t *pLock) {
#ifdef LINUX_WAY
    return (READ_ONCE(pLock->lock) != 0);
#else
    int r;
    r = (pLock->lock && pLock->cpu == cpuid());
    return r;
#endif
}

void spinlock_init(spinlock_t *pLock) {
    pLock->lock = 0;
    pLock->cpu  = 0;
}

#ifdef LINUX_WAY
static inline int spinlock_try_lock(spinlock_t *pLock) {
    int tmp = 1, busy;
    asm volatile("amoswap.w %0, %2, %1\n\t"
                 "fence r, rw"
                 : "=r"(busy), "+A"(pLock->lock)
                 : "r"(tmp)
                 : "memory");
    return !busy;
}
#endif

void spinlock_acquire(spinlock_t *pLock) {
    trap_push_off();
#ifdef LINUX_WAY
    while (1) {
        if (spinlock_holding(pLock))
            continue;
        if (spinlock_try_lock(pLock))
            break;
    }
#else
    if (spinlock_holding(pLock))
        kpanic("Acquired.");
    while (__sync_lock_test_and_set(&pLock->lock, 1) != 0)
        ;
    __sync_synchronize();
    pLock->cpu = cpuid();
#endif
}

void spinlock_release(spinlock_t *pLock) {
#ifdef LINUX_WAY
    RISCV_FENCE(rw, w);
    WRITE_ONCE(pLock->lock, 0);
#else
    if (!spinlock_holding(pLock))
        kpanic("Released.");
    pLock->cpu = 0;
    __sync_synchronize();
    __sync_lock_release(&pLock->lock);
#endif
    trap_pop_off();
}

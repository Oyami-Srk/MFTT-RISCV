//
// Created by shiroko on 22-5-4.
//

// From xv6

#include <lib/sys/sleeplock.h>
#include <proc.h>

void sleeplock_init(sleeplock_t *pLock) {
    pLock->lock = 0;
    spinlock_init(&pLock->spinlock);
}

void sleeplock_acquire(sleeplock_t *pLock) {
    spinlock_acquire(&pLock->spinlock);
    while (pLock->lock) {
        sleep(pLock, &pLock->spinlock);
    }
    pLock->lock = true;
    pLock->pid  = myproc()->pid;
    spinlock_release(&pLock->spinlock);
}

void sleeplock_release(sleeplock_t *pLock) {
    spinlock_acquire(&pLock->spinlock);
    pLock->lock = false;
    pLock->pid  = 0;
    wakeup(pLock);
    spinlock_release(&pLock->spinlock);
}

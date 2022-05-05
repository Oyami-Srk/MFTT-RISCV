//
// Created by shiroko on 22-5-5.
//

#include <proc.h>
#include <trap.h>

void sleep(void *chan, spinlock_t *lock) {
    proc_t *proc = myproc();
    if (lock != &proc->lock) {
        spinlock_acquire(&proc->lock);
        spinlock_release(lock);
    }
    proc->waiting_chan = chan;
    proc->status &= ~(PROC_STATUS_RUNNING | PROC_STATUS_READY);
    proc->status |= PROC_STATUS_WAITING;
    spinlock_release(&proc->lock);
    yield();
    spinlock_acquire(&proc->lock);
    proc->waiting_chan = NULL;
    if (lock != &proc->lock) {
        spinlock_release(&proc->lock);
        spinlock_acquire(lock);
    }
}

void wakeup(void *chan) {}

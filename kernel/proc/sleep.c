//
// Created by shiroko on 22-5-5.
//

#include <environment.h>
#include <proc.h>
#include <trap.h>

// lock is held when we call sleep
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

void wakeup(void *chan) {
    spinlock_acquire(&os_env.proc_lock);
    list_foreach_entry(&os_env.procs, proc_t, proc_list, proc) {
        spinlock_acquire(&proc->lock);
        if (proc->status & PROC_STATUS_WAITING && proc->waiting_chan == chan) {
            proc->status &= ~(PROC_STATUS_WAITING);
            proc->status |= (PROC_STATUS_READY | PROC_STATUS_NORMAL);
            // TODO: schedule the waiting process first.
        }
        spinlock_release(&proc->lock);
    }
    spinlock_release(&os_env.proc_lock);
}

//
// Created by shiroko on 22-5-1.
//

#include <environment.h>
#include <lib/sys/spinlock.h>
#include <proc.h>
#include <riscv.h>
#include <scheduler.h>

// Called in interrupt, if there is no process to switch
// return 1, otherwise return 0
int scheduler() {
    int found = 0;

    spinlock_acquire(&env.proc_lock);
    for (proc_t *proc = env.proc; proc < &env.proc[env.proc_count]; proc++) {
        spinlock_acquire(&proc->lock);
        if ((proc->status & (PROC_STATUS_NORMAL | PROC_STATUS_READY)) &&
            (proc->status & PROC_STATUS_RUNNING) == 0) {
            // Normal ready but not running
            proc->status &= ~PROC_STATUS_READY;
            proc->status |= PROC_STATUS_RUNNING;
            if (mycpu()->proc) {
                spinlock_acquire(&mycpu()->proc->lock);
                mycpu()->proc->status &= ~PROC_STATUS_RUNNING;
                mycpu()->proc->status |= PROC_STATUS_DONE;
                spinlock_release(&mycpu()->proc->lock);
            }
            mycpu()->proc = proc;
            found         = 1;
            spinlock_release(&proc->lock);
            break;
        }
        spinlock_release(&proc->lock);
    }
    if (found == 0) {
        proc_t *p = NULL;
        for (proc_t *proc = env.proc; proc < &env.proc[env.proc_count];
             proc++) {
            // new round
            if ((proc->status & (PROC_STATUS_NORMAL | PROC_STATUS_DONE))) {
                spinlock_acquire(&proc->lock);
                // Normal done but not ready
                proc->status &= ~PROC_STATUS_DONE;
                proc->status |= PROC_STATUS_READY;
                mycpu()->proc = proc;
                if (p == NULL) {
                    found = 1;
                    p     = proc;
                }
                spinlock_release(&proc->lock);
            }
        }
        if (found) {
            if (mycpu()->proc) {
                spinlock_acquire(&mycpu()->proc->lock);
                mycpu()->proc->status &= ~PROC_STATUS_RUNNING;
                mycpu()->proc->status |= PROC_STATUS_DONE;
                spinlock_release(&mycpu()->proc->lock);
            }
            mycpu()->proc = p;
        }
    }
    spinlock_release(&env.proc_lock);
    if (found == 0 && mycpu()->proc == NULL)
        return 1;
    return 0;
}

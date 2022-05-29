//
// Created by shiroko on 22-5-29.
//

#include <proc.h>
#include <trap.h>

void do_exit(proc_t *proc, int ec) {
    spinlock_acquire(&proc->lock);
    if (proc->pid == 1)
        kpanic("Init process cannot exit.");

    proc_free(proc);

    // reparent proc's child to init

    proc->exit_status = ec;
    proc->status      = PROC_STATUS_STOP;

    yield(); // never return.
}

pid_t do_wait(pid_t waitfor, int *status, int options) {}

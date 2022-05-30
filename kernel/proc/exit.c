//
// Created by shiroko on 22-5-29.
//

#include <environment.h>
#include <proc.h>
#include <stddef.h>
#include <trap.h>

void do_exit(proc_t *proc, int ec) {
    spinlock_acquire(&proc->lock);
    if (proc->pid == 1)
        kpanic("Init process cannot exit.");

    proc_free(proc);

    // reparent proc's child to init
    proc_t *parent = proc->parent;
    assert(parent, "Proc must have a parent.");
    spinlock_acquire(&parent->lock);
    list_foreach_entry(&proc->children, proc_t, child_list, child) {
        spinlock_acquire(&child->lock);
        child->parent = parent;
        list_add(&child->child_list, &parent->children);
        spinlock_release(&child->lock);
    }

    proc->exit_status = ec;
    proc->status      = PROC_STATUS_STOP;

    // wakeup parent if parent is waiting
    // there is like a manually wakeup function
    if (parent->status | PROC_STATUS_WAITING) {
        parent->status &= ~(PROC_STATUS_WAITING);
        parent->status |= (PROC_STATUS_READY | PROC_STATUS_NORMAL);
    }
    spinlock_release(&parent->lock);
    spinlock_release(&proc->lock);

    yield(); // never return.
}

pid_t do_wait(pid_t waitfor, int *status, int options) {
    proc_t *p = myproc();

    spinlock_acquire(&p->lock); // avoid wakeup miss
    while (true) {
        bool have_child = false;
        list_foreach_entry(&p->children, proc_t, child_list, child) {
            have_child = true;
            spinlock_acquire(&child->lock);
            if (child->status == PROC_STATUS_STOP) {
                // got a stopped child
                if (waitfor == -1 || waitfor == child->pid) {
                    // got the waiting one
                    *status = (int)child->exit_status;
                    // destory child'process
                    list_del(&child->child_list);
                    pid_t pid = child->pid;
                    kfree(child);
                    spinlock_acquire(&os_env.proc_lock);
                    clear_bit(os_env.proc_bitmap, pid);
                    set_proc(pid, NULL);
                    spinlock_release(&os_env.proc_lock);
                    spinlock_release(&child->lock);
                    spinlock_release(&p->lock);
                    return pid;
                }
            }
            spinlock_release(&child->lock);
        }
        if (options == WNOHANG) {
            return 0; // imm return
        } else if (!have_child) {
            // no child, return
            spinlock_release(&p->lock);
            return -1;
        }
        // got child, wait for child
        sleep(p, &p->lock);
    }
}

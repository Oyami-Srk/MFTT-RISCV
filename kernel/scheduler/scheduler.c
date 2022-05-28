//
// Created by shiroko on 22-5-1.
//

#include <environment.h>
#include <lib/sys/spinlock.h>
#include <proc.h>
#include <riscv.h>
#include <scheduler.h>

// 简易的轮换调度实现
// TODO: More flexable scheduler
proc_t *scheduler(scheduler_data_t *data) {
    proc_t *ret_proc = NULL;
#define PROC_RUNNABLE (PROC_STATUS_READY | PROC_STATUS_NORMAL)
    // 锁定进程表
    spinlock_acquire(&os_env.proc_lock);
    // 锁定调度器数据
    spinlock_acquire(&data->lock);

    /*
    for (proc_t *proc = &os_env.proc[data->last_scheduled_pid + 1];
         proc < &os_env.proc[os_env.proc_count]; proc++) {
        spinlock_acquire(&proc->lock);
        if (((proc->status & PROC_RUNNABLE) == PROC_RUNNABLE) &&
            (proc->status & PROC_STATUS_RUNNING) == 0) {
            // Normal ready but not running, Could be scheduled
            ret_proc                 = proc; // with lock
            data->last_scheduled_pid = proc->pid;
            goto ret;
        }
        spinlock_release(&proc->lock);
    }
    for (proc_t *proc = os_env.proc;
         proc <= &os_env.proc[data->last_scheduled_pid]; proc++) {
        // next round
        spinlock_acquire(&proc->lock);
        if (((proc->status & PROC_RUNNABLE) == PROC_RUNNABLE) &&
            (proc->status & PROC_STATUS_RUNNING) == 0) {
            // Normal ready but not running, Could be scheduled
            ret_proc                 = proc; // with lock
            data->last_scheduled_pid = proc->pid;
            goto ret;
        }
        spinlock_release(&proc->lock);
    } */
    if (data->last_scheduled_proc) {
        list_foreach_entry(&data->last_scheduled_proc->proc_list, proc_t,
                           proc_list, proc) {
            spinlock_acquire(&proc->lock);
            if (((proc->status & PROC_RUNNABLE) == PROC_RUNNABLE) &&
                (proc->status & PROC_STATUS_RUNNING) == 0) {
                // Normal ready but not running, Could be scheduled
                ret_proc                  = proc; // with lock
                data->last_scheduled_proc = proc;
                goto ret;
            }
            spinlock_release(&proc->lock);
            if (proc->proc_list.next == &os_env.procs)
                break;
        }
    }
    list_foreach_entry(&os_env.procs, proc_t, proc_list, proc) {
        spinlock_acquire(&proc->lock);
        if (((proc->status & PROC_RUNNABLE) == PROC_RUNNABLE) &&
            (proc->status & PROC_STATUS_RUNNING) == 0) {
            // Normal ready but not running, Could be scheduled
            ret_proc                  = proc; // with lock
            data->last_scheduled_proc = proc;
            goto ret;
        }
        spinlock_release(&proc->lock);
        if (proc == data->last_scheduled_proc)
            break;
    }
ret:
    spinlock_release(&data->lock);
    spinlock_release(&os_env.proc_lock);
    return ret_proc;
}

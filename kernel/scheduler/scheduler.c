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

    if (data->last_scheduled_proc) {
        list_foreach_entry(&data->last_scheduled_proc->proc_list, proc_t,
                           proc_list, proc) {
            if (&proc->proc_list == &os_env.procs)
                break;
            spinlock_acquire(&proc->lock);
            if (((proc->status & PROC_RUNNABLE) == PROC_RUNNABLE) &&
                (proc->status & PROC_STATUS_RUNNING) == 0) {
                // Normal ready but not running, Could be scheduled
                ret_proc                  = proc; // with lock
                data->last_scheduled_proc = proc;
                goto ret;
            }
            spinlock_release(&proc->lock);
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

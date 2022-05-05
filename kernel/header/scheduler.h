//
// Created by shiroko on 22-5-1.
//

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <lib/sys/spinlock.h>
#include <proc.h>

// scheduler data is environment data, not pre-CPU data
// must lock for access.
typedef struct {
    spinlock_t lock;
    pid_t      last_scheduled_pid;
} scheduler_data_t;

// Return process to be scheduled
// NULL represent no process could be scheduled
// process lock is hold before return.
// scheduler must change process status
proc_t *scheduler(scheduler_data_t *data);

#endif // __SCHEDULER_H__
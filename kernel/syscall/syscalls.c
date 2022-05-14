//
// Created by shiroko on 22-5-6.
//

#include <environment.h>
#include <lib/stdlib.h>
#include <lib/sys/spinlock.h>
#include <syscall.h>

// Syscall is running in proc context with S-privilge

// SYS_ticks: 返回系统节拍器时间
sysret_t sys_ticks(struct trap_context *trapframe) {
    uint64_t ret;
    spinlock_acquire(&env.ticks_lock);
    ret = env.ticks;
    spinlock_release(&env.ticks_lock);
    return ret;
}

#include <driver/console.h>
sysret_t sys_print(struct trap_context *trapframe) {
    char *buffer = (char *)trapframe->a0;
    assert(buffer, "Print with null buffer.");
    kprintf("%s", buffer);
    return 0;
}

sysret_t sys_sleep(struct trap_context *trapframe) {
    uint64_t t     = (uint64_t)trapframe->a0;
    uint64_t ticks = 0;
    spinlock_acquire(&env.ticks_lock);
    ticks = env.ticks;
    while (env.ticks - ticks < t) {
        if (myproc()->status & PROC_STATUS_STOP) {
            spinlock_release(&env.ticks_lock);
            return -1;
        }
        sleep(&env.ticks, &env.ticks_lock);
    }
    spinlock_release(&env.ticks_lock);
    return 0;
}

// Syscall table
static sysret_t (*syscall_table[])(struct trap_context *) = {
    [SYS_ticks] = sys_ticks,
    [SYS_print] = sys_print,
    [SYS_sleep] = sys_sleep,
};

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
#include <driver/console.h>

void do_syscall(struct trap_context *trapframe) {
    CSR_RWOR(sstatus, SSTATUS_SUM);
    int syscall_id = trapframe->a7;
    if (unlikely(syscall_id >= NELEM(syscall_table) ||
                 syscall_table[syscall_id] == NULL)) {
        assert(myproc(), "Proc invalid.");
        kprintf("PID %d calls invalid syscall with id %d.\n", myproc()->pid,
                syscall_id);
        trapframe->a0 = -1;
        return;
    }
    // TODO: strace
    trapframe->a0 = syscall_table[syscall_id](trapframe);
    CSR_RWAND(sstatus, ~SSTATUS_SUM);
}
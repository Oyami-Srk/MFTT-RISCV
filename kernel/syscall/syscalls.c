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

sysret_t sys_print(struct trap_context *trapframe) {}

// Syscall table
static sysret_t (*syscall_table[])(struct trap_context *) = {
    [SYS_ticks] = sys_ticks,
    [SYS_print] = sys_print,
};

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
#include <driver/console.h>

void do_syscall(struct trap_context *trapframe) {
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
}
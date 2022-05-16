//
// Created by shiroko on 22-5-6.
//

#include <environment.h>
#include <lib/stdlib.h>
#include <lib/sys/spinlock.h>
#include <syscall.h>
#include <vfs.h>

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

sysret_t sys_open(struct trap_context *trapframe) {
    int         fd       = (int)(trapframe->a0 & 0xFFFFFFFF);
    const char *filename = (const char *)(trapframe->a1);
    int         flags    = (int)(trapframe->a3 & 0xFFFFFFFF);
    int         mode     = (int)(trapframe->a4 & 0xFFFFFFFF);

    int                   new_fd = 1;
    struct vfs_dir_entry *dentry = vfs_get_dentry(filename, NULL);
    if (!dentry)
        return -1;
    struct vfs_file *file = vfs_open(dentry, mode);
    if (!file)
        return -1;
    struct vfs_file **ftable = myproc()->files;
    for (int i = 0; i < MAX_FILE_OPEN; i++) {
        if (ftable[i] == NULL) {
            ftable[i] = file;
            return i;
        }
    }
    vfs_close(file);
    return -1;
}

sysret_t sys_close(struct trap_context *trapframe) {
    int fd = (int)(trapframe->a0 & 0xFFFFFFFF);
    return 0;
}

sysret_t sys_read(struct trap_context *trapframe) {
    int              fd    = (int)(trapframe->a0 & 0xFFFFFFFF);
    char            *buf   = (char *)(trapframe->a1);
    size_t           count = (size_t)(trapframe->a2);
    struct vfs_file *file  = myproc()->files[fd];
    if (!file)
        return -1;
    return vfs_read(file, buf, file->f_offset, count);
}

sysret_t sys_write(struct trap_context *trapframe) {
    int              fd    = (int)(trapframe->a0 & 0xFFFFFFFF);
    const char      *buf   = (const char *)(trapframe->a1);
    size_t           count = (size_t)(trapframe->a2);
    struct vfs_file *file  = myproc()->files[fd];
    if (!file)
        return -1;
    return vfs_write(file, buf, file->f_offset, count);
}

// Syscall table
// clang-format off
static sysret_t (*syscall_table[])(struct trap_context *) = {
    [SYS_ticks] = sys_ticks,
    [SYS_print] = sys_print,
    [SYS_sleep] = sys_sleep,

    [SYS_openat] = sys_open,
    [SYS_close] = sys_close,
    [SYS_write] = sys_write,
    [SYS_read] = sys_read,
};
// clang-format on

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

#include <driver/console.h>

void do_syscall(struct trap_context *trapframe) {
    // TODO: move sum mark into trap handler
    CSR_RWOR(sstatus, SSTATUS_SUM);
    int syscall_id = (int)(trapframe->a7 & 0xFFFFFFFF);
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
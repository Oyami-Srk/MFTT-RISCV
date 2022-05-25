//
// Created by shiroko on 22-5-6.
//

#include <environment.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/sys/spinlock.h>
#include <stddef.h>
#include <syscall.h>
#include <vfs.h>

// Syscall is running in proc context with S-privilge

// SYS_ticks: 返回系统节拍器时间
sysret_t sys_ticks(struct trap_context *trapframe) {
    uint64_t ret;
    spinlock_acquire(&os_env.ticks_lock);
    ret = os_env.ticks;
    spinlock_release(&os_env.ticks_lock);
    return ret;
}

sysret_t sys_sleep(struct trap_context *trapframe) {
    uint64_t t     = (uint64_t)trapframe->a0;
    uint64_t ticks = 0;
    spinlock_acquire(&os_env.ticks_lock);
    ticks = os_env.ticks;
    while (os_env.ticks - ticks < t) {
        if (myproc()->status & PROC_STATUS_STOP) {
            spinlock_release(&os_env.ticks_lock);
            return -1;
        }
        sleep(&os_env.ticks, &os_env.ticks_lock);
    }
    spinlock_release(&os_env.ticks_lock);
    return 0;
}

sysret_t sys_open(struct trap_context *trapframe) {
    int   parent_fd = (int)(trapframe->a0 & 0xFFFFFFFF);
    char *filename  = ustrcpy_out((char *)(trapframe->a1));
    if (!filename)
        return -1;
    int flags = (int)(trapframe->a3 & 0xFFFFFFFF);
    int mode  = (int)(trapframe->a4 & 0xFFFFFFFF);

    dentry_t *cwd = NULL;
    if (parent_fd == AT_FDCWD)
        cwd = myproc()->cwd;
    else {
        file_t **ftable = myproc()->files;
        if (ftable[parent_fd])
            cwd = ftable[parent_fd]->f_dentry;
        else {
            return -2;
        }
    }

    dentry_t *dentry = vfs_get_dentry(filename, cwd);
    kfree(filename);
    if (!dentry)
        return -1;
    file_t *file = vfs_open(dentry, mode);
    if (!file)
        return -1;
    file_t **ftable = myproc()->files;
    for (int i = 3; i < MAX_FILE_OPEN; i++) {
        if (ftable[i] == NULL) {
            ftable[i] = file;
            return i;
        }
    }
    vfs_close(file);
    return -1;
}

sysret_t sys_close(struct trap_context *trapframe) {
    int      fd      = (int)(trapframe->a0 & 0xFFFFFFFF);
    file_t **ftables = myproc()->files;
    if (ftables[fd]) {
        vfs_close(ftables[fd]);
        ftables[fd] = NULL;
        return 0;
    } else {
        return -1;
    }
    return 0;
}

sysret_t sys_read(struct trap_context *trapframe) {
    int     fd    = (int)(trapframe->a0 & 0xFFFFFFFF);
    char   *buf   = (char *)(trapframe->a1);
    size_t  bytes = (size_t)(trapframe->a2);
    file_t *file  = myproc()->files[fd];
    if (!file)
        return -1;
    char *kbuf = NULL;
    if (bytes < PG_SIZE)
        kbuf = (char *)kmalloc(bytes);
    else
        kbuf = (char *)page_alloc(PG_ROUNDUP(bytes), PAGE_TYPE_SYSTEM);
    if (!kbuf)
        return -1;
    int r = vfs_read(file, kbuf, 0, bytes);
    umemcpy(buf, kbuf, bytes);
    if (bytes < PG_SIZE)
        kfree(kbuf);
    else
        page_free(kbuf, PG_ROUNDUP(bytes));
    return r;
}

sysret_t sys_write(struct trap_context *trapframe) {
    int         fd    = (int)(trapframe->a0 & 0xFFFFFFFF);
    const char *buf   = (const char *)(trapframe->a1);
    size_t      bytes = (size_t)(trapframe->a2);
    file_t     *file  = myproc()->files[fd];
    if (!file)
        return -1;
    char *kbuf = NULL;
    if (bytes < PG_SIZE)
        kbuf = (char *)kmalloc(bytes);
    else
        kbuf = (char *)page_alloc(PG_ROUNDUP(bytes), PAGE_TYPE_SYSTEM);
    if (!kbuf)
        return -1;
    umemcpy(kbuf, buf, bytes);
    int r = vfs_write(file, kbuf, 0, bytes);
    if (bytes < PG_SIZE)
        kfree(kbuf);
    else
        page_free(kbuf, PG_ROUNDUP(bytes));
    return r;
}

sysret_t sys_lseek(struct trap_context *trapframe) {
    int    fd     = (int)trapframe->a0;
    size_t offset = trapframe->a1;
    int    whence = (int)trapframe->a2;

    file_t **ftables = myproc()->files;
    if (ftables[fd]) {
        file_t *f = ftables[fd];
        return (sysret_t)vfs_lseek(f, offset, whence);
    } else {
        return -1;
    }
}

sysret_t sys_clone(struct trap_context *trapframe) {
    int   flags = (int)trapframe->a0;
    char *stack = (char *)trapframe->a1;
    int   ptid  = (int)trapframe->a2;
    int   tls   = (int)trapframe->a3;
    int   ctid  = (int)trapframe->a4;
    if (flags == SIGCHLD) {
        proc_t *parent = myproc();
        return do_fork(parent);
    } else {
        // TODO: impl this
    }
    trapframe->a0 = -1;
}

sysret_t sys_getppid(struct trap_context *trapframe) {
    proc_t *proc = myproc();
    if (proc->parent)
        return proc->parent->pid;
    return 0;
}

sysret_t sys_mkdirat(struct trap_context *trapframe) {
    int   fd       = (int)trapframe->a0;
    char *filename = ustrcpy_out((char *)(trapframe->a1));
    if (!filename)
        return -1;
    int mode = (int)trapframe->a1;

    file_t  **ftables = myproc()->files;
    dentry_t *d       = NULL;
    if (fd == AT_FDCWD) {
        d = myproc()->cwd;
    } else if (ftables[fd]) {
        d = ftables[fd]->f_dentry;
    } else {
        kfree(filename);
        return -1;
    }
    if (!d) {
        kfree(filename);
        return -2;
    }

    dentry_t *dent = vfs_mkdir(d, filename, mode);
    kfree(filename);
    if (!dent)
        return -2;
    return 0;
}

sysret_t sys_mount(struct trap_context *trapframe) {
    char *dev        = ustrcpy_out((char *)trapframe->a0);
    char *mountpoint = ustrcpy_out((char *)trapframe->a1);
    char *fstype     = ustrcpy_out((char *)trapframe->a2);
    void *flags      = (void *)trapframe->a3;
    // TODO: FS declare flags size and kernel copy in kspace, currently NULL
    int r = vfs_mount(dev, mountpoint, fstype, flags);
    kfree(fstype);
    kfree(mountpoint);
    kfree(dev);
    return r;
}

sysret_t sys_getdents64(struct trap_context *trapframe) {
    int       fd     = (int)trapframe->a0;
    dirent_t *dirent = (dirent_t *)trapframe->a1;
    size_t    len    = (size_t)trapframe->a2;

    // get file
    file_t *f = NULL;
    if (fd == AT_FDCWD) {
        return 0; // AT_FDCWD not supported here
    } else {
        f = myproc()->files[fd];
        if (!f)
            return 0; // can't read
    }
    if (f->f_dentry->d_type != D_TYPE_DIR &&
        f->f_dentry->d_type != D_TYPE_MOUNTED)
        return 0; // no dir
    // start read dir
    char *kbuf = kmalloc(len);
    assert(kbuf, "Out of memory when getdents.");
    char              *end_kbuf = kbuf + len;
    read_dir_context_t ctx;
    char              *p = kbuf;

    for (; (char *)p < end_kbuf;) {
        void *prev_fs_data = f->f_fs_data;
        if (vfs_read_dir(f, &ctx) < 0)
            break; // no more ent
        size_t namelen = strlen(ctx.d_name);
        if (p + sizeof(dirent_t) + namelen >= end_kbuf) {
            f->f_fs_data = prev_fs_data;
            break; // no more space
        }
        dirent_t *d = (dirent_t *)p;
        d->d_ino    = ctx.d_inode->i_ino;
        d->d_reclen = 0;
        d->d_off    = 0;
        switch (ctx.d_inode->i_type) {
        case inode_dev:
            d->d_type = 'S';
            break;
        case inode_dir:
            d->d_type = 'D';
            break;
        case inode_file:
            d->d_type = 'F';
            break;
        default:
            d->d_type = 'U';
            break;
        }
        strcpy(d->d_name, ctx.d_name);
        p += sizeof(dirent_t) + namelen;
    }
    size_t sz_read = p - kbuf;
    umemcpy(dirent, kbuf, sz_read);
    kfree(kbuf);
    return sz_read;
}

static int count_strs(const char **strs) {
    int r = 0;
    for (; strs[r] != NULL; r++)
        ;
    return r;
}

sysret_t sys_execve(struct trap_context *trapframe) {
    const char  *path = ustrcpy_out((char *)trapframe->a0);
    char *const *argv = (char *const *)trapframe->a1;
    char *const *envp = (char *const *)trapframe->a2;
    CSR_RWOR(sstatus, SSTATUS_SUM);
    int    argc  = count_strs((const char **)argv);
    int    envc  = count_strs((const char **)envp);
    char **kargv = (char **)kmalloc(sizeof(char *) * argc);
    char **kenvp = (char **)kmalloc(sizeof(char *) * envc);
    assert(kargv, "out of memory.");
    assert(kenvp, "out of memory.");
    kargv[argc] = kenvp[envc] = NULL;
    for (int i = 0; i < argc; i++) {
        kargv[i] = ustrcpy_out(argv[i]);
    }
    for (int i = 0; i < envc; i++) {
        kenvp[i] = ustrcpy_out(envp[i]);
    }
    CSR_RWAND(sstatus, ~SSTATUS_SUM);
    // do execve
    int r = do_execve(myproc(), myproc()->cwd, path, (const char **)kargv,
                      (const char **)kenvp);
    // free resource
    for (int i = 0; i < envc; i++) {
        kfree(kenvp[i]);
    }
    for (int i = 0; i < argc; i++) {
        kfree(kargv[i]);
    }
    kfree(kenvp);
    kfree(kargv);
    kfree((char *)path);
    // return argc
    assert(r == argc, "argc not identical while do_execve.");
    return r;
}

// Syscall table
extern sysret_t sys_test(struct trap_context *);
// clang-format off
static sysret_t (*syscall_table[])(struct trap_context *) = {
    [SYS_ticks] = sys_ticks,
    [SYS_print] = NULL,
    [SYS_sleep] = sys_sleep,
    [SYS_test] = sys_test, // inside test.c, remove when stable

    [SYS_openat] = sys_open,
    [SYS_close] = sys_close,
    [SYS_write] = sys_write,
    [SYS_read] = sys_read,
    [SYS_lseek] = sys_lseek,
    [SYS_getcwd]= NULL,
    [SYS_pipe2]= NULL,
    [SYS_dup]= NULL,
    [SYS_dup3]= NULL,
    [SYS_chdir]= NULL,
    [SYS_getdents64]= sys_getdents64,
    [SYS_linkat]= NULL,
    [SYS_unlinkat]= NULL,
    [SYS_mkdirat]= sys_mkdirat,
    [SYS_umount2]= NULL,
    [SYS_mount]= sys_mount,
    [SYS_fstat]= NULL,
    [SYS_clone]= sys_clone,
    [SYS_execve]= sys_execve,
    [SYS_wait4]= NULL,
    [SYS_exit]= NULL,
    [SYS_getppid]= sys_getppid,
    [SYS_getpid]= NULL,
    [SYS_brk]= NULL,
    [SYS_munmap]= NULL,
    [SYS_mmap]= NULL,
    [SYS_times]= NULL,
    [SYS_uname]= NULL,
    [SYS_sched_yield]= NULL,
    [SYS_gettimeofday]= NULL,
    [SYS_nanosleep]= NULL,
};
// clang-format on

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

#include <driver/console.h>

void do_syscall(struct trap_context *trapframe) {
    // TODO: move sum mark into trap handler
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
    sysret_t ret  = syscall_table[syscall_id](trapframe);
    trapframe->a0 = ret;
}
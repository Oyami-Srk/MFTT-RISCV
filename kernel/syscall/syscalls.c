//
// Created by shiroko on 22-5-6.
//

#include <dev/pipe.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/sys/spinlock.h>
#include <stddef.h>
#include <sys_structs.h>
#include <syscall.h>
#include <trap.h>
#include <vfs.h>

// Syscall is running in proc context with S-privilge

#define TRACE_SYSCALL 0

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
    int     fd   = (int)(trapframe->a0 & 0xFFFFFFFF);
    proc_t *proc = myproc();
    file_t *file = proc->files[fd];
    if (file) {
        vfs_close(file);
        proc->files[fd] = NULL;
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
    int      fd     = (int)trapframe->a0;
    offset_t offset = (offset_t)trapframe->a1;
    int      whence = (int)trapframe->a2;

    proc_t *proc = myproc();
    file_t *file = proc->files[fd];
    if (file) {
        return (sysret_t)vfs_lseek(file, offset, whence);
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
        return do_fork(parent, stack);
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

    proc_t   *proc   = myproc();
    file_t  **ftable = proc->files;
    dentry_t *d      = NULL;
    if (fd == AT_FDCWD) {
        d = proc->cwd;
    } else if (ftable[fd]) {
        d = ftable[fd]->f_dentry;
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

        if (ROUNDUP_WITH(sizeof(uint32_t), p + sizeof(dirent_t) + namelen) >=
            (uintptr_t)end_kbuf) {
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
        p = (char *)ROUNDUP_WITH(sizeof(uint64_t), p);
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

    int argc = 0;
    int envc = 0;
    BEGIN_UMEM_ACCESS();
    if (argv)
        argc = count_strs((const char **)argv);
    if (envp)
        envc = count_strs((const char **)envp);
    char **kargv = (char **)kmalloc(sizeof(char *) * (argc + 1));
    char **kenvp = (char **)kmalloc(sizeof(char *) * (envc + 1));
    assert(kargv, "out of memory.");
    assert(kenvp, "out of memory.");

    kargv[argc] = kenvp[envc] = NULL;
    for (int i = 0; i < argc; i++) {
        kargv[i] = ustrcpy_out(argv[i]);
    }
    for (int i = 0; i < envc; i++) {
        kenvp[i] = ustrcpy_out(envp[i]);
    }
    STOP_UMEM_ACCESS();

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
    if (r < 0)
        return r;
    assert(r == argc, "argc not identical while do_execve.");
    return r;
}

sysret_t sys_brk(struct trap_context *trapframe) {
    uintptr_t brk_va = (uintptr_t)(trapframe->a0);
    return do_brk(myproc(), brk_va);
}

sysret_t sys_getpid(struct trap_context *trapframe) { return myproc()->pid; }

sysret_t sys_exit(struct trap_context *trapframe) {
    do_exit(myproc(), (int)trapframe->a0);
    kpanic("Never reach here.");
}

sysret_t sys_wait(struct trap_context *trapframe) {
    pid_t waitfor  = (pid_t)trapframe->a0;
    int  *u_status = (int *)trapframe->a1;
    int   options  = (int)trapframe->a2;
    int   k_status = 0;
    pid_t r        = do_wait(waitfor, &k_status, options);
    if (u_status)
        umemcpy(u_status, &k_status, sizeof(int));
    return r;
}

sysret_t sys_sched_yield(struct trap_context *trapframe) {
    proc_t *proc = myproc();
    proc->status &= ~PROC_STATUS_RUNNING;
    proc->status |= PROC_STATUS_READY;
    yield();
    return 0;
}

static struct utsname build_uts_name = {.sysname  = "MFTT-RISCV",
                                        .nodename = "",
                                        .release  = "0.1.0",
                                        .version  = __TIME__ __DATE__,
                                        .machine  = "riscv64"};

sysret_t sys_uname(struct trap_context *trapframe) {
    struct utsname *uts = (struct utsname *)(trapframe->a0);
    umemcpy(uts, &build_uts_name, sizeof(struct utsname));
    return 0;
}

sysret_t sys_getcwd(struct trap_context *trapframe) {
    char  *ubuf = (char *)trapframe->a0;
    size_t size = (size_t)trapframe->a1;

    proc_t *proc = myproc();
    char   *kbuf = vfs_get_dentry_fullpath(proc->cwd);
    size_t  len  = strlen(kbuf) + 1;
    if (size < len)
        return (sysret_t)NULL; // buffer to small
    else {
        char *ret = NULL;
        if (ubuf) {
            umemcpy(ubuf, kbuf, len);
            ret = ubuf;
        } else {
            // alloc by our
            char *old_brk = (char *)ROUNDUP_WITH(8, proc->prog_break);
            do_brk(proc, ROUNDUP_WITH(8, old_brk + len));
            umemcpy(old_brk, kbuf, len);
            ret = old_brk;
        }
        kfree(kbuf);
        return (sysret_t)ret;
    }
}

sysret_t sys_chdir(struct trap_context *trapframe) {
    char *path = ustrcpy_out((char *)trapframe->a0);

    proc_t   *proc    = myproc();
    dentry_t *new_cwd = vfs_get_dentry(path, proc->cwd);
    sysret_t  r       = 0;
    if (new_cwd) {
        proc->cwd = new_cwd;
        r         = 0;
    } else {
        r = -1;
    }

    kfree(path);
    return r;
}

sysret_t sys_pipe2(struct trap_context *trapframe) {
    int    *ufds      = (int *)trapframe->a0;
    int     kfds[2]   = {0, 0};
    file_t *kfiles[2] = {NULL, NULL};

    file_t **ftable = myproc()->files;
    int      co     = 0;
    for (int i = 3; i < MAX_FILE_OPEN; i++) {
        if (ftable[i] == NULL) {
            file_t *file = (file_t *)kmalloc(sizeof(file_t));
            if (!file) {
                goto failed;
            }
            memset(file, 0, sizeof(file_t));
            ftable[i]  = file;
            kfiles[co] = file;
            kfds[co++] = i;
            if (co == 2)
                break;
        }
    }
    if (kfds[1] == 0)
        goto failed;

    int r = pipe_create(kfiles[0], kfiles[1]);
    if (r != 0)
        goto failed;

    umemcpy(ufds, kfds, sizeof(int) * 2);
    return 0;
failed:
    for (int i = 0; i < 2; i++)
        if (kfds[i]) {
            file_t *file = ftable[kfds[i]];
            if (file->f_fs_data && file->f_op)
                file->f_op->close(file);
            ftable[kfds[i]] = NULL;
            kfree(file);
        }
    return -1;
}

sysret_t sys_dup2(struct trap_context *trapframe) {
    int old_fd = (int)trapframe->a0;
    int new_fd = -1;

    file_t **ftable = myproc()->files;
    if (ftable[old_fd] == NULL)
        return -1;
    for (int i = 3; i < MAX_FILE_OPEN; i++) {
        if (ftable[i] == NULL) {
            new_fd = i;
            break;
        }
    }
    if (new_fd == -1)
        return -1;
    ftable[new_fd] = vfs_fdup(ftable[old_fd]);

    return new_fd;
}

sysret_t sys_dup3(struct trap_context *trapframe) {
    int old_fd = (int)trapframe->a0;
    int new_fd = (int)trapframe->a1;

    file_t **ftable = myproc()->files;
    if (ftable[old_fd] == NULL)
        return -1;
    if (ftable[new_fd] != NULL)
        vfs_close(ftable[new_fd]);
    ftable[new_fd] = vfs_fdup(ftable[old_fd]);

    return new_fd;
}

sysret_t sys_fstat(struct trap_context *trapframe) {
    int      fd     = (int)trapframe->a0;
    kstat_t *ukstat = (kstat_t *)trapframe->a1;
    if (!ukstat)
        return -1;
    file_t *f = myproc()->files[fd];
    if (!f)
        return -1;
    kstat_t kkstat = {
        .st_dev        = f->f_inode->i_dev[1],
        .st_ino        = f->f_inode->i_ino,
        .st_mode       = f->f_mode,
        .st_nlink      = f->f_inode->i_nlinks,
        .st_uid        = 0,
        .st_gid        = 0,
        .st_rdev       = f->f_inode->i_dev[0],
        .st_size       = f->f_inode->i_size,
        .st_blksize    = 0,
        .st_blocks     = 0,
        .st_atime_sec  = f->f_inode->i_atime,
        .st_atime_nsec = 0,
        .st_mtime_sec  = f->f_inode->i_mtime,
        .st_mtime_nsec = 0,
        .st_ctime_sec  = f->f_inode->i_ctime,
        .st_ctime_nsec = 0,
    };
    umemcpy(ukstat, &kkstat, sizeof(kstat_t));
    return 0;
}

sysret_t sys_gettimeofday(struct trap_context *trapframe) {
    struct timespec *utime = (struct timespec *)trapframe->a0;
    if (!utime)
        return -1;
    struct timespec ktime = {.tv_sec = sys_ticks(NULL), .tv_nsec = 0};
    umemcpy(utime, &ktime, sizeof(struct timespec));
    return 0;
}

sysret_t sys_times(struct trap_context *trapframe) {
    struct tms *utms = (struct tms *)trapframe->a0;
    if (!utms)
        return -1;
    uint64_t   ticks = sys_ticks(NULL);
    struct tms ktms  = {
         .tms_utime  = 0, // User cpu time
         .tms_stime  = 0, // System cpu time
         .tms_cutime = 0, // User cpu time of children
         .tms_cstime = 0, // System cpu time of children
    };
    return ticks;
}

sysret_t sys_nanosleep(struct trap_context *trapframe) {
    struct timespec *uts = (struct timespec *)trapframe->a0;
    if (!uts)
        return -1;
    struct timespec kts;
    umemcpy(&kts, uts, sizeof(struct timespec));
    uint64_t ticks_to_sleep = kts.tv_sec;

    uint64_t ticks = 0;
    spinlock_acquire(&os_env.ticks_lock);
    ticks = os_env.ticks;
    while (os_env.ticks - ticks < ticks_to_sleep) {
        if (myproc()->status & PROC_STATUS_STOP) {
            spinlock_release(&os_env.ticks_lock);
            return -1;
        }
        sleep(&os_env.ticks, &os_env.ticks_lock);
    }
    spinlock_release(&os_env.ticks_lock);
    return 0;
}

extern sysret_t sys_test(struct trap_context *);
// Syscall table
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
    [SYS_getcwd]= sys_getcwd,
    [SYS_pipe2]= sys_pipe2,
    [SYS_dup]= sys_dup2,
    [SYS_dup3]= sys_dup3,
    [SYS_chdir]= sys_chdir,
    [SYS_getdents64]= sys_getdents64,
    [SYS_linkat]= NULL,
    [SYS_unlinkat]= NULL,
    [SYS_mkdirat]= sys_mkdirat,
    [SYS_umount2]= NULL,
    [SYS_mount]= sys_mount,
    [SYS_fstat]= sys_fstat,
    [SYS_clone]= sys_clone,
    [SYS_execve]= sys_execve,
    [SYS_wait4]= sys_wait,
    [SYS_exit]= sys_exit,
    [SYS_getppid]= sys_getppid,
    [SYS_getpid]= sys_getpid,
    [SYS_brk]= sys_brk,
    [SYS_munmap]= NULL,
    [SYS_mmap]= NULL,
    [SYS_times]= sys_times,
    [SYS_uname]= sys_uname,
    [SYS_sched_yield]= sys_sched_yield,
    [SYS_gettimeofday]= sys_gettimeofday,
    [SYS_nanosleep]= sys_nanosleep,
};


static const char* syscall_names[] = {
    [SYS_ticks] = "SYS_ticks",
    [SYS_print] = "SYS_print",
    [SYS_sleep] = "SYS_sleep",
    [SYS_test] = "SYS_test", // inside test.c, remove when stable

    [SYS_openat] = "SYS_openat",
    [SYS_close] = "SYS_close",
    [SYS_write] = "SYS_write",
    [SYS_read] = "SYS_read",
    [SYS_lseek] = "SYS_lseek",
    [SYS_getcwd] = "SYS_getcwd",
    [SYS_pipe2] = "SYS_pipe2",
    [SYS_dup] = "SYS_dup",
    [SYS_dup3] = "SYS_dup3",
    [SYS_chdir] = "SYS_chdir",
    [SYS_getdents64] = "SYS_getdents64",
    [SYS_linkat] = "SYS_linkat",
    [SYS_unlinkat] = "SYS_unlinkat",
    [SYS_mkdirat] = "SYS_mkdirat",
    [SYS_umount2] = "SYS_umount2",
    [SYS_mount] = "SYS_mount",
    [SYS_fstat] = "SYS_fstat",
    [SYS_clone] = "SYS_clone",
    [SYS_execve] = "SYS_execve",
    [SYS_wait4] = "SYS_wait4",
    [SYS_exit] = "SYS_exit",
    [SYS_getppid] = "SYS_getppid",
    [SYS_getpid] = "SYS_getpid",
    [SYS_brk] = "SYS_brk",
    [SYS_munmap] = "SYS_munmap",
    [SYS_mmap] = "SYS_mmap",
    [SYS_times] = "SYS_times",
    [SYS_uname] = "SYS_uname",
    [SYS_sched_yield] = "SYS_sched_yield",
    [SYS_gettimeofday] = "SYS_gettimeofday",
    [SYS_nanosleep] = "SYS_nanosleep",
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
        do_exit(myproc(), -1);
        // trapframe->a0 = -1;
        return;
    }
    // TODO: strace
#if TRACE_SYSCALL
    proc_t *proc = myproc();
    kprintf("\n;;;;;;;; Proc [%d]%s Call %s\n", proc->pid, proc->name,
            syscall_names[syscall_id]);
#endif
    sysret_t ret  = syscall_table[syscall_id](trapframe);
    trapframe->a0 = ret;
}
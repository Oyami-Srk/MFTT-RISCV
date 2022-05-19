//
// Created by shiroko on 22-5-6.
//

#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <common/types.h>

typedef long long sysret_t;

#define SYSCALL_USE_OSCOMP
#define SYSCALL_USE_MFTT

#ifdef SYSCALL_USE_MFTT
#define SYS_ticks 1
#define SYS_print 2
#define SYS_sleep 3
#define SYS_test  2333
#endif

#ifdef SYSCALL_USE_OSCOMP
// Syscall for os comp
#define SYS_getcwd       17
#define SYS_pipe2        59
#define SYS_dup          23
#define SYS_dup3         24
#define SYS_chdir        49
#define SYS_openat       56
#define SYS_close        57
#define SYS_getdents64   61
#define SYS_read         63
#define SYS_write        64
#define SYS_linkat       37
#define SYS_unlinkat     35
#define SYS_mkdirat      34
#define SYS_umount2      39
#define SYS_mount        40
#define SYS_fstat        80
#define SYS_clone        220
#define SYS_execve       221
#define SYS_wait4        260
#define SYS_exit         93
#define SYS_getppid      173
#define SYS_getpid       172
#define SYS_brk          214
#define SYS_munmap       215
#define SYS_mmap         222
#define SYS_times        153
#define SYS_uname        160
#define SYS_sched_yield  124
#define SYS_gettimeofday 169
#define SYS_nanosleep    101
#endif

void do_syscall(struct trap_context *trapframe);

#endif // __SYSCALL_H__
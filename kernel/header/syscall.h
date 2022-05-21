//
// Created by shiroko on 22-5-6.
//

#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <syscall_nums.h>
#include <types.h>

typedef long long sysret_t;

void do_syscall(struct trap_context *trapframe);

#endif // __SYSCALL_H__
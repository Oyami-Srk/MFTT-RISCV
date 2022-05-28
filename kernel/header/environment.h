//
// Created by shiroko on 22-4-28.
//

#ifndef __ENVIRONMENT_H__
#define __ENVIRONMENT_H__

#include <configs.h>
#include <lib/linklist.h>
#include <lib/sys/spinlock.h>
#include <memory.h>
#include <proc.h>
#include <riscv.h>
#include <scheduler.h>
#include <types.h>

struct __cpu_t {
    // int     cpuid; // This is inside the register tp
    proc_t             *proc;
    int                 trap_off_depth;
    bool                trap_enabled;
    struct task_context context;
};
typedef struct __cpu_t cpu_t;

#define ENV_BEGIN_GUARD 0x20010125
#define ENV_END_GUARD   0xC0DEBABE

struct __env_t {
    uint64_t begin_gaurd;
    /* CPUs */
    cpu_t cpus[MAX_CPUS] __attribute__((aligned(8)));
    /* Timer */
    uint64_t   ticks;
    spinlock_t ticks_lock;
    /* Memory */
    // TODO: consider move memory_info to here
    pde_t       kernel_pagedir;
    uint64_t    kernel_satp;
    char       *kernel_boot_stack;
    char       *kernel_boot_stack_top;
    list_head_t mem_sysmaps;
    /* Process */
    list_head_t procs;
    // proc_t           proc[MAX_PROC];
    size_t           proc_count;
    bitset_t         proc_bitmap[BITSET_ARRAY_SIZE_FOR(MAX_PROC)];
    spinlock_t       proc_lock;
    scheduler_data_t scheduler_data;
    /* VFS */
    /* Device */
    list_head_t driver_list_head;
    /* Interrupt */

    uint64_t end_gaurd;
} __attribute__((aligned(16)));
typedef struct __env_t env_t;

// TODO: I hate global value
extern env_t os_env; // Inside environment.c

static ALWAYS_INLINE inline cpu_t *mycpu() { return &os_env.cpus[cpuid()]; }

void init_env();

#endif // __ENVIRONMENT_H__
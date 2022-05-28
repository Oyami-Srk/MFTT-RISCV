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

struct __env_t {
    /* CPUs */
    cpu_t cpus[MAX_CPUS];
    /* Timer */
    uint64_t   ticks;
    spinlock_t ticks_lock;
    /* Memory */
    // TODO: consider move memory_info to here
    pde_t       kernel_pagedir;
    uint64_t    kernel_satp;
    list_head_t mem_sysmaps;
    /* Process */
    // TODO: make process table dynamicly allocated and increase.
    // proc_t *proc[MAX_PROC];
    proc_t           proc[MAX_PROC];
    size_t           proc_count;
    bitset_t         proc_bitmap[BITSET_ARRAY_SIZE_FOR(MAX_PROC)];
    spinlock_t       proc_lock;
    scheduler_data_t scheduler_data;
    /* VFS */
    /* Device */
    list_head_t driver_list_head;
    /* Interrupt */
} __attribute__((aligned(16)));
typedef struct __env_t env_t;

// TODO: I hate global value
extern env_t os_env; // Inside environment.c

static ALWAYS_INLINE inline cpu_t *mycpu() { return &os_env.cpus[cpuid()]; }

void init_env();

#endif // __ENVIRONMENT_H__
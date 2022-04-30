//
// Created by shiroko on 22-4-28.
//

#ifndef __ENVIRONMENT_H__
#define __ENVIRONMENT_H__

#include <common/types.h>
#include <configs.h>
#include <lib/sys/spinlock.h>
#include <memory.h>
#include <proc.h>
#include <riscv.h>

struct __cpu_t {
    // int     cpuid; // This is inside the register tp
    proc_t *proc;
    int     trap_off_depth;
    bool    trap_enabled;
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
    pde_t kernel_pagedir;
    /* Process */
    // TODO: make process table dynamicly allocated and increase.
    // proc_t *proc[MAX_PROC];
    proc_t proc[MAX_PROC];
    size_t proc_count;
    bitset
        proc_bitmap[MAX_PROC / BITS_PER_BITSET]; // for fast finding empty slot
    spinlock_t proc_lock;
} __attribute__((aligned(16)));
typedef struct __env_t env_t;

// TODO: I hate global value
extern env_t env; // Inside startup.c

static inline cpu_t *mycpu() { return &env.cpus[cpuid()]; }

#endif // __ENVIRONMENT_H__
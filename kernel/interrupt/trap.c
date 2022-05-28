#include "./utils.h"
#include <configs.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/sys/SBI.h>
#include <memory.h>
#include <riscv.h>
#include <smp_barrier.h>
#include <trap.h>
#include <types.h>

void init_trap() {
    set_interrupt_to_kernel();
    enable_trap();
    CSR_RWOR(sie, SIE_SEIE | SIE_SSIE | SIE_STIE);
    SBI_set_timer(cpu_cycle() + TIMER_COUNTER);
}

void trap_push_off() {
    // Ensure we don't active interrupt if cpu not intended to enable it
    bool previous_status_of_sie = CSR_Read(sstatus) & SSTATUS_SIE;
    disable_trap();
    int    my_cpuid = (int)cpuid();
    cpu_t *cpu      = &os_env.cpus[my_cpuid];
    if (cpu->trap_off_depth == 0)
        cpu->trap_enabled = previous_status_of_sie;
    cpu->trap_off_depth++;
}

void trap_pop_off() {
    assert((CSR_Read(sstatus) & SSTATUS_SIE) == 0,
           "Want pop interrupt but already enabled it.");
    int    my_cpuid = (int)cpuid();
    cpu_t *cpu      = &os_env.cpus[my_cpuid];
    assert(cpu->trap_off_depth, "No need to pop.");
    cpu->trap_off_depth--;
    if (cpu->trap_off_depth == 0 && mycpu()->trap_enabled)
        enable_trap();
}

// Exception panic

static bool exception_lock = false;

static void printf_nolock(const char *fmt, ...) {
    int         i;
    static char buf[512];
    va_list     arg;
    va_start(arg, fmt);
    i       = vsprintf(buf, fmt, arg);
    buf[i]  = 0;
    char *s = buf;
    while (*s != '\0')
        SBI_putchar(*(s++));
}

static const char *exception_description[] = {
    "Instruction address misaligned.",
    "Instruction access fault.",
    "Illegal instruction.",
    "Breakpoint.",
    "Reserved.",
    "Load access fault.",
    "AMO address misaligned.",
    "Store/AMO access fault.",
    "Environment call.",
    "Reserved.",
    "Reserved.",
    "Reserved.",
    "Instruction page fault.",
    "Load page fault.",
    "Reserved.",
    "Store/AMO page fault.",
};

void dump_trapframe(struct trap_context *tf) {
    printf_nolock("   ra: %18lp\t", tf->ra);
    printf_nolock("   sp: %18lp\n", tf->sp);
    printf_nolock("   gp: %18lp\t", tf->gp);
    printf_nolock("   tp: %18lp\n", tf->tp);

    printf_nolock("   a0: %18lp\t", tf->a0);
    printf_nolock("   a1: %18lp\n", tf->a1);
    printf_nolock("   a2: %18lp\t", tf->a2);
    printf_nolock("   a3: %18lp\n", tf->a3);
    printf_nolock("   a4: %18lp\t", tf->a4);
    printf_nolock("   a5: %18lp\n", tf->a5);
    printf_nolock("   a6: %18lp\t", tf->a6);
    printf_nolock("   a7: %18lp\n", tf->a7);

    printf_nolock("   t0: %18lp\t", tf->t0);
    printf_nolock("   t1: %18lp\n", tf->t1);
    printf_nolock("   t2: %18lp\t", tf->t2);
    printf_nolock("   t3: %18lp\n", tf->t3);
    printf_nolock("   t4: %18lp\t", tf->t4);
    printf_nolock("   t5: %18lp\n", tf->t5);
    printf_nolock("   t6: %18lp\n", tf->t6);

    printf_nolock("   s0: %18lp\t", tf->s0);
    printf_nolock("   s1: %18lp\n", tf->s1);
    printf_nolock("   s2: %18lp\t", tf->s2);
    printf_nolock("   s3: %18lp\n", tf->s3);
    printf_nolock("   s4: %18lp\t", tf->s4);
    printf_nolock("   s5: %18lp\n", tf->s5);
    printf_nolock("   s6: %18lp\t", tf->s6);
    printf_nolock("   s7: %18lp\n", tf->s7);
    printf_nolock("   s8: %18lp\t", tf->s8);
    printf_nolock("   s9: %18lp\n", tf->s9);
    printf_nolock("  s10: %18lp\t", tf->s10);
    printf_nolock("  s11: %18lp\n", tf->s11);
}

static inline int exception_try_lock() {
    int tmp = 1, busy;
    asm volatile("amoswap.w %0, %2, %1\n\t"
                 "fence r, rw"
                 : "=r"(busy), "+A"(exception_lock)
                 : "r"(tmp)
                 : "memory");
    return !busy;
}

static inline void lock_exception() {
    while (1) {
        if ((READ_ONCE(exception_lock) != 0))
            continue;
        if (exception_try_lock())
            break;
    }
}

void print_sstatus(uint64_t sstatus) {
    printf_nolock("sstatus: ");
    bool p = false;
#define PRINT(x)                                                               \
    do {                                                                       \
        if (sstatus & SSTATUS_##x) {                                           \
            if (p == true)                                                     \
                printf_nolock("|");                                            \
            else                                                               \
                p = true;                                                      \
            printf_nolock(#x);                                                 \
        }                                                                      \
    } while (0)
    PRINT(UIE);
    PRINT(SIE);
    PRINT(UPIE);
    PRINT(SPIE);
    PRINT(SPP);
#if PLATFORM_QEMU
    PRINT(SUM);
    PRINT(MXR);
#else
    PRINT(PUM);
#endif
    PRINT(SD);
#define V(x) ((sstatus >> SSTATUS_##x##_SHIFT) & 0x3)
    printf_nolock("    FS: %d, XS: %d", V(FS), V(XS));
#if PLATFORM_QEMU
    printf_nolock(", UXL: %d.\n", V(UXL));
#else
    printf_nolock(".\n");
#endif
}

void exception_panic(uint64_t scause, uint64_t stval, uint64_t sepc,
                     uint64_t sstatus, struct trap_context *trapframe) {
    // lock
    lock_exception();

    printf_nolock("\n\n====================CPU %d=======================\n\n",
                  cpuid());
    printf_nolock("Exception %d: %s\n", scause,
                  scause < 16 ? exception_description[scause]
                              : exception_description[4]);
    proc_t *proc = os_env.cpus[cpuid()].proc;
    if (proc) {
        printf_nolock("In Process: %d.\n", proc->pid);
    }
    print_sstatus(sstatus);
    printf_nolock(" sepc: %18lp\tstval: %18lp\n", sepc, stval);
    dump_trapframe(trapframe);
    printf_nolock("KERN_BASE: %lp\tKERN_END: %lp\n", KERN_BASE, KERN_END);
    printf_nolock("Kernel Page Dir: %p\n", os_env.kernel_pagedir);
    printf_nolock("Kernel Stack: %p ~ %p\nKernel Code: %p ~ %p",
                  os_env.kernel_boot_stack, os_env.kernel_boot_stack_top,
                  KERN_CODE_START, KERN_CODE_END);
    printf_nolock("Env Guard: %p, %p\n", os_env.begin_gaurd, os_env.end_gaurd);
    printf_nolock("\n================================================\n\n",
                  cpuid());
    // release lock
    RISCV_FENCE(rw, w);
    WRITE_ONCE(exception_lock, 0);
}
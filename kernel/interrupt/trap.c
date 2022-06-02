#include "./utils.h"
#include <configs.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/string.h>
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

static inline void put_char(const char c) { SBI_putchar(c); }

static inline void put_str(const char *s) {
    while (*s != '\0')
        put_char(*(s++));
}

static inline void put_str_aligned(const char *s, int align) {
    size_t len = strlen(s);
    if (len < align)
        while (align - (len++))
            put_char(' ');
    put_str(s);
}

#define PRINT_HEX_ALIGN(name, x, terminator, align)                            \
    do {                                                                       \
        char buffer[32] = {[0] = '0', [1] = 'x', [2 ... 31] = 0};              \
        put_str_aligned(name ": ", 7);                                         \
        itoa((long long)(x), buffer + 2, 16);                                  \
        put_str_aligned(buffer, align);                                        \
        put_char((terminator));                                                \
    } while (0)

#define PRINT_HEX(name, x, terminator) PRINT_HEX_ALIGN(name, x, terminator, 18);

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
    PRINT_HEX("ra", tf->ra, '\t');
    PRINT_HEX("sp", tf->sp, '\n');
    PRINT_HEX("gp", tf->gp, '\t');
    PRINT_HEX("tp", tf->tp, '\n');

    PRINT_HEX("a0", tf->a0, '\t');
    PRINT_HEX("a1", tf->a1, '\n');
    PRINT_HEX("a2", tf->a2, '\t');
    PRINT_HEX("a3", tf->a3, '\n');
    PRINT_HEX("a4", tf->a4, '\t');
    PRINT_HEX("a5", tf->a5, '\n');
    PRINT_HEX("a6", tf->a6, '\t');
    PRINT_HEX("a7", tf->a7, '\n');

    PRINT_HEX("t0", tf->t0, '\t');
    PRINT_HEX("t1", tf->t1, '\n');
    PRINT_HEX("t2", tf->t2, '\t');
    PRINT_HEX("t3", tf->t3, '\n');
    PRINT_HEX("t4", tf->t4, '\t');
    PRINT_HEX("t5", tf->t5, '\n');
    PRINT_HEX("t6", tf->t6, '\n');

    PRINT_HEX("s0", tf->s0, '\t');
    PRINT_HEX("s1", tf->s1, '\n');
    PRINT_HEX("s2", tf->s2, '\t');
    PRINT_HEX("s3", tf->s3, '\n');
    PRINT_HEX("s4", tf->s4, '\t');
    PRINT_HEX("s5", tf->s5, '\n');
    PRINT_HEX("s6", tf->s6, '\t');
    PRINT_HEX("s7", tf->s7, '\n');
    PRINT_HEX("s8", tf->s8, '\t');
    PRINT_HEX("s9", tf->s9, '\n');
    PRINT_HEX("s10", tf->s10, '\t');
    PRINT_HEX("s11", tf->s11, '\n');
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
    put_str("sstatus: ");
    bool p = false;
#define PRINT(x)                                                               \
    do {                                                                       \
        if (sstatus & SSTATUS_##x) {                                           \
            if (p == true)                                                     \
                put_str("|");                                                  \
            else                                                               \
                p = true;                                                      \
            put_str(#x);                                                       \
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
    put_str("    FS: ");
    put_char(V(FS) + '0');
    put_str(", XS: ");
    put_char(V(XS) + '0');
#if PLATFORM_QEMU
    put_str(", UXL: ");
    put_char(V(UXL) + '0');
    put_char('\n');
#else
    put_char('\n');
#endif
}

void exception_panic(uint64_t scause, uint64_t stval, uint64_t sepc,
                     uint64_t sstatus, struct trap_context *trapframe) {
    disable_trap();
    // lock
    lock_exception();
    uint64_t cpu = cpuid();
    put_str("\n\n====================CPU ");
    put_char(cpu + '0');
    put_str("=======================\n\n");

    put_str("Exception ");
    do {
        char buffer[32] = {[0 ... 31] = 0};
        itoa((long long)scause, buffer, 10);
        put_str(buffer);
    } while (0);
    put_str(": ");
    if (scause < 16) {
        put_str(exception_description[scause]);
    } else {
        put_str(exception_description[4]);
    }
    put_char('\n');

    proc_t *proc = os_env.cpus[cpu].proc;
    if (proc) {
        put_str("Happend with process [");
        put_char(proc->pid + '0');
        put_char(']');
        put_str(proc->name);
        put_char('\n');
    }
    print_sstatus(sstatus);

    PRINT_HEX("sepc", sepc, '\t');
    PRINT_HEX("stval", stval, '\n');

    dump_trapframe(trapframe);
    PRINT_HEX_ALIGN("KERN_BASE", KERN_BASE, '\t', 0);
    PRINT_HEX_ALIGN("KERN_END", KERN_END, '\n', 0);
    PRINT_HEX_ALIGN("Kernel Page Dir", os_env.kernel_pagedir, '\n', 0);
    do {
        put_str("Kernel Stack: 0x");
        char buffer[32] = {[0 ... 31] = 0};
        itoa((long long)os_env.kernel_boot_stack, buffer, 16);
        put_str(buffer);
        put_str(" ~ 0x");
        itoa((long long)os_env.kernel_boot_stack_top, buffer, 16);
        put_str(buffer);
        put_str("\n");

        put_str("Kernel Code: 0x");
        itoa((long long)KERN_CODE_START, buffer, 16);
        put_str(buffer);
        put_str(" ~ 0x");
        itoa((long long)KERN_CODE_END, buffer, 16);
        put_str(buffer);
        put_str("\n");
    } while (0);

    PRINT_HEX_ALIGN("Env Canary Begin", os_env.begin_gaurd, '\n', 0);
    PRINT_HEX_ALIGN("Env Canary End", os_env.end_gaurd, '\n', 0);

    put_str("\n================================================\n\n");
    // release lock
    RISCV_FENCE(rw, w);
    WRITE_ONCE(exception_lock, 0);
}
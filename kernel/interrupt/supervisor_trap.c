//
// Created by shiroko on 22-4-30.
//

#include <common/types.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/sys/SBI.h>
#include <riscv.h>
#include <trap.h>

spinlock_t exception_lock = {.cpu = 0, .lock = 0};

static void exception_printf(const char *fmt, ...) {
    int     i;
    char    buf[1024];
    va_list arg;
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
    exception_printf("a0: 0x%lx\t", tf->a0);
    exception_printf("a1: 0x%lx\t", tf->a1);
    exception_printf("a2: 0x%lx\t", tf->a2);
    exception_printf("a3: 0x%lx\n", tf->a3);
    exception_printf("a4: 0x%lx\t", tf->a4);
    exception_printf("a5: 0x%lx\t", tf->a5);
    exception_printf("a6: 0x%lx\t", tf->a6);
    exception_printf("a7: 0x%lx\n", tf->a7);
    exception_printf("t0: 0x%lx\t", tf->t0);
    exception_printf("t1: 0x%lx\t", tf->t1);
    exception_printf("t2: 0x%lx\t", tf->t2);
    exception_printf("t3: 0x%lx\n", tf->t3);
    exception_printf("t4: 0x%lx\t", tf->t4);
    exception_printf("t5: 0x%lx\t", tf->t5);
    exception_printf("t6: 0x%lx\t", tf->t6);
    exception_printf("s0: 0x%lx\n", tf->s0);
    exception_printf("s1: 0x%lx\t", tf->s1);
    exception_printf("s2: 0x%lx\t", tf->s2);
    exception_printf("s3: 0x%lx\t", tf->s3);
    exception_printf("s4: 0x%lx\n", tf->s4);
    exception_printf("s5: 0x%lx\t", tf->s5);
    exception_printf("s6: 0x%lx\t", tf->s6);
    exception_printf("s7: 0x%lx\t", tf->s7);
    exception_printf("s8: 0x%lx\n", tf->s8);
    exception_printf("s9: 0x%lx\t", tf->s9);
    exception_printf("s10: 0x%lx\t", tf->s10);
    exception_printf("s11: 0x%lx\t", tf->s11);
    exception_printf("ra: 0x%lx\n", tf->ra);
    exception_printf("sp: 0x%lx\t", tf->sp);
    exception_printf("gp: 0x%lx\t", tf->gp);
    exception_printf("tp: 0x%lx\n", tf->tp);
}

// Used in trap.S
void __attribute__((used)) supervisor_trap_handler(struct trap_context *tf) {
    // TODO: Validate trap source and status are correct.
    uint64_t scause  = CSR_Read(scause);
    uint64_t stval   = CSR_Read(stval);
    uint64_t sepc    = CSR_Read(sepc);
    uint64_t sstatus = CSR_Read(sstatus);

    assert((sstatus & SSTATUS_SPP), "Supervisor trap must from kernel.");
    assert((sstatus & SSTATUS_SIE) == 0,
           "Trap triggered with interrupt enabled.");

    if (scause & XCAUSE_INT) {
        handle_interrupt(scause & 0x7FFFFFFFFFFFFFFF);
    } else {
        //        spinlock_acquire(&exception_lock);
        // cause by exception
        exception_printf(
            "\n\n====================CPU %d=======================\n\n",
            cpuid());
        //        dump_trapframe(tf);
        exception_printf("Exception %d[%d]: %s Caused by code at 0x%lx with "
                         "stval: 0x%lx.\n",
                         scause, cpuid(),
                         scause < 16 ? exception_description[scause]
                                     : exception_description[4],
                         sepc, stval);
        if (myproc()) {
            proc_t *proc = myproc();
            exception_printf("In Process: %d.\n", proc->pid);
        }
        exception_printf(
            "\n\n================================================\n\n",
            cpuid());
        //        spinlock_release(&exception_lock);
        //        SBI_shutdown();
        while (1)
            ;
    }
    CSR_Write(sepc, sepc);
    CSR_Write(sstatus, sstatus);
}

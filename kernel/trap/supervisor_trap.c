//
// Created by shiroko on 22-4-30.
//

#include <common/types.h>
#include <driver/SBI.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <riscv.h>
#include <trap.h>

extern void timer_tick(); // timer.c

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

// Used in trap.S
void __attribute__((used)) supervisor_trap_handler() {
    // TODO: Validate trap source and status are correct.
    uint64_t scause  = CSR_Read(scause);
    uint64_t stval   = CSR_Read(stval);
    uint64_t sepc    = CSR_Read(sepc);
    uint64_t sstatus = CSR_Read(sstatus);

    if (scause & XCAUSE_INT) {
        int type = scause & 0x7FFFFFFFFFFFFFFF;
        // caused by interrupt
        if (type == 5) {
            // timer interrupt
            if (cpuid() == 0)
                timer_tick();
            SBI_set_timer(cpu_cycle() + 7800000);
        }
    } else {
        // cause by exception
        exception_printf("Exception %d[%d]: %s Caused by code at 0x%lx with "
                         "stval: 0x%lx.\n",
                         scause, cpuid(),
                         scause < 16 ? exception_description[scause]
                                     : exception_description[4],
                         sepc, stval);
        SBI_shutdown();
        while (1)
            ;
    }
}

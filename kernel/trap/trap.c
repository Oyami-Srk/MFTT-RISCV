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
    "Environment call",
};

extern void supervisor_interrupt_vector(); // trap.S

void init_trap() {
    assert((((uint64_t)supervisor_interrupt_vector) & 0x3) == 0,
           "Interrupt Vector must be 4bytes aligned.");
    // stvec: Supervisor Trap-Vector Base Address, low two bits are mode.
    CSR_Write(stvec,
              ((uint64_t)supervisor_interrupt_vector & (~0x3)) | TRAP_MODE);
    if (cpuid() == 0)
        CSR_RWOR(sie, SIE_STIE);
    CSR_RWOR(sie, SIE_SEIE | SIE_SSIE);
    // test timer
    SBI_set_timer(cpu_cycle() + 7800000);

    enable_trap();
}

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
            timer_tick();
            SBI_set_timer(cpu_cycle() + 7800000);
        }
    } else {
        // cause by exception
        if (likely(scause < 9)) {
            exception_printf("Exception[%d]: %s Caused by code at 0x%lx.\n",
                             cpuid(), exception_description[scause], sepc);
        } else {
            exception_printf("Exception code reserved.");
        }
        SBI_shutdown();
        while (1)
            ;
    }
}

void __attribute__((used)) user_trap_handler() {}

void trap_push_off() {
    // Ensure we don't active interrupt if cpu not intended to enable it
    bool previous_status_of_sie = CSR_Read(sstatus) & SSTATUS_SIE;
    disable_trap();
    if (mycpu()->trap_off_depth == 0)
        mycpu()->trap_enabled = previous_status_of_sie;
    mycpu()->trap_off_depth++;
}

void trap_pop_off() {
    assert((CSR_Read(sstatus) & SSTATUS_SIE) == 0,
           "Want pop interrupt but already enabled it.");
    assert(mycpu()->trap_off_depth, "No need to pop.");
    mycpu()->trap_off_depth--;
    if (mycpu()->trap_off_depth == 0 && mycpu()->trap_enabled)
        enable_trap();
}

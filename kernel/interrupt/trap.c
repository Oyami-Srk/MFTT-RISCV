#include <types.h>
#include "./utils.h"
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/sys/SBI.h>
#include <riscv.h>
#include <trap.h>

void init_trap() {
    set_interrupt_to_kernel();
    enable_trap();
    CSR_RWOR(sie, SIE_SEIE | SIE_SSIE | SIE_STIE);
    SBI_set_timer(cpu_cycle() + 7800000);
}

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

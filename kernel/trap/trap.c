#include <common/types.h>
#include <driver/SBI.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <riscv.h>
#include <trap.h>

extern void supervisor_interrupt_vector(); // trap.S

void init_trap() {
    assert((((uint64_t)supervisor_interrupt_vector) & 0x3) == 0,
           "Interrupt Vector must be 4bytes aligned.");
    // stvec: Supervisor Trap-Vector Base Address, low two bits are mode.
    CSR_Write(stvec,
              ((uint64_t)supervisor_interrupt_vector & (~0x3)) | TRAP_MODE);
    CSR_RWOR(sie, SIE_SEIE | SIE_SSIE | SIE_STIE);
    // test timer
    SBI_set_timer(cpu_cycle() + 7800000);

    enable_trap();
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

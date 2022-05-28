//
// Created by shiroko on 22-4-30.
//

#include "./utils.h"
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/sys/SBI.h>
#include <riscv.h>
#include <trap.h>
#include <types.h>

void system_do_reset() { SBI_ext_srst(); }

// Used in trap.S
void __attribute__((used)) supervisor_trap_handler(struct trap_context *tf) {
    // TODO: Validate trap source and status are correct.
    uint64_t scause  = CSR_Read(scause);
    uint64_t stval   = CSR_Read(stval);
    uint64_t sepc    = CSR_Read(sepc);
    uint64_t sstatus = CSR_Read(sstatus);
    uint64_t satp    = CSR_Read(satp);

    assert((sstatus & SSTATUS_SPP), "Supervisor trap must from kernel.");
    if ((sstatus & SSTATUS_SIE) != 0) {
        CSR_RWAND(sstatus, ~SSTATUS_SIE);
    }
    /*
    assert((sstatus & SSTATUS_SIE) == 0,
           "Trap triggered with interrupt enabled.");*/

    if (scause & XCAUSE_INT) {
        handle_interrupt(scause & 0x7FFFFFFFFFFFFFFF);
    } else {
        // cause by exception
        if (COULD_BE_PAGEFAULT(scause) && stval < 0x80000000) {
            if (do_pagefault(
                    (char *)stval,
                    (pde_t)((CSR_Read(satp) & 0xFFFFFFFFFFF) << PG_SHIFT),
                    true) != 0) {
                kpanic("do page fault failed.\n");
            }
        } else {
            exception_panic(scause, stval, sepc, sstatus, tf);
            while (1)
                ;
            SBI_ext_srst();
        }
    }
    CSR_Write(sepc, sepc);
    if (CSR_Read(sstatus) & SSTATUS_SUM)
        CSR_Write(sstatus, sstatus | SSTATUS_SUM);
    else
        CSR_Write(sstatus, sstatus);
}

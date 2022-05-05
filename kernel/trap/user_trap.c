//
// Created by shiroko on 22-4-30.
//

#include "./utils.h"
#include <common/types.h>
#include <driver/SBI.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <riscv.h>
#include <trap.h>

// Used in trap.S
void __attribute__((used)) user_trap_handler(proc_t *proc) {
    set_interrupt_to_kernel();
    // TODO: Validate trap source and status are correct.
    uint64_t scause  = CSR_Read(scause);
    uint64_t stval   = CSR_Read(stval);
    uint64_t sepc    = CSR_Read(sepc);
    uint64_t sstatus = CSR_Read(sstatus);

    if (scause & XCAUSE_INT) {
        // interrupt
        handle_interrupt(scause & 0x7FFFFFFFFFFFFFFF);
    } else if (scause == 8) {
        // syscall
        kprintf("Call: %d.\n", proc->trapframe.a0);
        proc->user_pc += 4;
    } else {
        // cause by exception
        while (1)
            ;
    }
    user_trap_return();
}

extern void user_ret(proc_t *proc);

// return to user space with process
void user_trap_return() {
    disable_trap();
    proc_t *proc = mycpu()->proc;
    assert(proc, "Process must be valid.");
    set_interrupt_to_user();
    proc->kernel_sp    = proc->kernel_stack + PG_SIZE;
    proc->kernel_cpuid = cpuid();
    uint64_t sstatus   = CSR_Read(sstatus);
    // clear SPP for user mode to make interrupt funcional
    sstatus &= ~SSTATUS_SPP;
    // set user mode interrupt enable
    sstatus |= SSTATUS_SPIE;
    CSR_Write(sstatus, sstatus);
    CSR_Write(sepc, proc->user_pc);

    user_ret(proc);
}

void return_to_cpu_process() {
    // switch back process context
    proc_t *proc = myproc();
    cpu_t  *cpu  = mycpu();
    assert(proc, "Process must be valid.");
    context_switch(&mycpu()->context, &mycpu()->proc->kernel_task_context);
}
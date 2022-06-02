//
// Created by shiroko on 22-4-30.
//

#include "./utils.h"
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/sys/SBI.h>
#include <riscv.h>
#include <syscall.h>
#include <trap.h>
#include <types.h>

// Used in trap.S
void __attribute__((used)) user_trap_handler(proc_t *proc) {
    set_interrupt_to_kernel();
    // TODO: Validate trap source and status are correct.
    uint64_t scause  = CSR_Read(scause);
    uint64_t stval   = CSR_Read(stval);
    uint64_t sepc    = CSR_Read(sepc);
    uint64_t sstatus = CSR_Read(sstatus);
    uint64_t satp    = CSR_Read(satp);

    if (scause & XCAUSE_INT) {
        // interrupt
        handle_interrupt(scause & 0x7FFFFFFFFFFFFFFF);
    } else if (scause == 8) {
        // do syscall
        // TODO: Check op type
        proc->user_pc += 4;
        // TODO: maybe a lock here
        do_syscall(&proc->trapframe);
    } else {
        // cause by exception

        if (COULD_BE_PAGEFAULT(scause) && stval < 0x80000000) {
            // page fault.
            if (do_pagefault(
                    (char *)stval,
                    (pde_t)((CSR_Read(satp) & 0xFFFFFFFFFFF) << PG_SHIFT),
                    false) != 0) {
                kprintf("do page fault failed.\n");

                exception_panic(scause, stval, sepc, sstatus, &proc->trapframe);
                kprintf("Kill process [%d]%s.\n", proc->pid, proc->name);
                if (proc) {
                    do_exit(proc, -1);
                }
            }
        } else {
            exception_panic(scause, stval, sepc, sstatus, &proc->trapframe);
            /*
            while (1)
                ;
            SBI_ext_srst(); */
            kprintf("Kill process [%d]%s.\n", proc->pid, proc->name);
            if (proc) {
                do_exit(proc, -1);
            }
        }
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
    proc->kernel_sp    = proc->kernel_stack_top;
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
    // switch to proc kernel page
    CSR_Write(satp, proc->page_csr);
    context_switch(&mycpu()->context, &mycpu()->proc->kernel_task_context);
}
//
// Created by shiroko on 22-5-2.
//

#include <environment.h>
#include <lib/stdlib.h>
#include <proc.h>
#include <riscv.h>
#include <trap.h>

void switch_to_scheduler() {
    bool trap_enabled = mycpu()->trap_enabled;
    assert(mycpu()->proc, "Proc must be valid.");
    context_switch(&mycpu()->proc->kernel_task_context, &mycpu()->context);
    mycpu()->trap_enabled = trap_enabled;
}

void yield() { switch_to_scheduler(); }
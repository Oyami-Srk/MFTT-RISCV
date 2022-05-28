//
// Created by shiroko on 22-5-2.
//

#include <environment.h>
#include <lib/stdlib.h>
#include <proc.h>
#include <riscv.h>
#include <trap.h>

void yield() {
    int    my_cpuid     = (int)cpuid();
    cpu_t *cpu          = &os_env.cpus[my_cpuid];
    bool   trap_enabled = cpu->trap_enabled;
    //    bool trap_enabled = mycpu()->trap_enabled;
    assert(cpu->proc, "Proc must be valid.");
    struct task_context *old = &cpu->proc->kernel_task_context;
    struct task_context *new = &cpu->context;
    assert(old, "Old context null.");
    assert(new, "New context null.");

    // context_switch(&cpu->proc->kernel_task_context, &cpu->context);
    context_switch(old, new);

    my_cpuid          = (int)cpuid();
    cpu               = &os_env.cpus[my_cpuid];
    cpu->trap_enabled = trap_enabled;
}
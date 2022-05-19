//
// Created by shiroko on 22-4-26.
//

#ifndef __TRAP_H__
#define __TRAP_H__

#include <proc.h>

#define TRAP_MODE_DIRECT   0x00
#define TRAP_MODE_VECTORED 0x01
#define TRAP_MODE          TRAP_MODE_DIRECT
#define MAX_INTERRUPT      64

void init_trap();
void init_plic(); // plic.c
void handle_interrupt(uint64_t cause);

void trap_push_off();
void trap_pop_off();

// switch.S
void context_switch(struct task_context *old, struct task_context *new);
// yield.c
void yield();
// user_trap.c
void user_trap_return();
void return_to_cpu_process();
// plic.c
int plic_register_irq(int irq);
// interrupt.c, register and unreg ext-int
typedef int (*interrupt_handler_t)(void);
int interrupt_try_reg(int interrupt, interrupt_handler_t handler);
int interrupt_try_unreg(int interrupt, interrupt_handler_t handler);

#endif // __TRAP_H__
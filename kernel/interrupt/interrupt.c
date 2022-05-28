//
// Created by shiroko on 22-5-2.
//

#include "./plic.h"
#include <configs.h>
#include <lib/sys/SBI.h>
#include <riscv.h>
#include <scheduler.h>
#include <trap.h>
#include <types.h>

extern void timer_tick(); // timer.c

static interrupt_handler_t int_handlers[MAX_INTERRUPT] = {0};

void handle_interrupt(uint64_t cause) {
    if (cause == 5) {
        // timer interrupt
        // 1. master core increase the tick
        if (cpuid() == 0)
            timer_tick();
        // 2. set next timer.
        SBI_set_timer(cpu_cycle() + TIMER_COUNTER);
        // 3. yield cpu for schelder running if there is any process.
        if (myproc() && myproc()->status & PROC_STATUS_RUNNING) {
            spinlock_acquire(&myproc()->lock);
            if (myproc()->status & PROC_STATUS_RUNNING) {
                myproc()->status &= ~PROC_STATUS_RUNNING;
                myproc()->status |= PROC_STATUS_READY;
            }
            spinlock_release(&myproc()->lock);
            yield();
        }
    }
    // TODO: THIS IS UGLY, I HATE COMPILE TIME DEFINATION FOR HARDWARE
#if USE_SOFT_INT_COMP
    else if (cause == 1 && CSR_Read(stval) == 9) {
        // RustSBI simulated interrupt, stval == 9 is ext-int (for K210)
#else
    else if (cause == 9) {
        // according to priviliged 1.10, cause = 9 is Supervisor ext-int
#endif
        int irq = plic_begin();
        if (irq) {
            if (int_handlers[irq]) {
                int _result = (int_handlers[irq])();
                // TODO: validate interrupt result
            }
            plic_end(irq);
        }

#if USE_SOFT_INT_COMP
        CSR_RWAND(sip, ~2); // clear pending bit
        SBI_set_mie();
#endif
    }
}

int interrupt_try_reg(int interrupt, interrupt_handler_t handler) {
    if (interrupt >= MAX_INTERRUPT)
        return -2;
    if (int_handlers[interrupt] == NULL) {
        int_handlers[interrupt] = handler;
        return 0;
    } else {
        if (int_handlers[interrupt] == handler)
            return 0;
        return -1;
    }
}

int interrupt_try_unreg(int interrupt, interrupt_handler_t handler) {
    if (interrupt >= MAX_INTERRUPT)
        return -2;
    if (int_handlers[interrupt]) {
        if (int_handlers[interrupt] == handler) {
            int_handlers[interrupt] = NULL;
            return 0;
        }
        return -1;
    }
    return 0;
}

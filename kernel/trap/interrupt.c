//
// Created by shiroko on 22-5-2.
//

#include <common/types.h>
#include <driver/SBI.h>
#include <riscv.h>
#include <scheduler.h>
#include <trap.h>

extern void timer_tick(); // timer.c

void handle_interrupt(uint64_t cause) {
    if (cause == 5) {
        // timer interrupt
        // 1. master core increase the tick
        if (cpuid() == 0)
            timer_tick();
        // 2. set next timer.
        SBI_set_timer(cpu_cycle() + 7800000);
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
}
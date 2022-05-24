#include <environment.h>
#include <proc.h>

void timer_tick() {
    spinlock_acquire(&os_env.ticks_lock);
    os_env.ticks++;
    spinlock_release(&os_env.ticks_lock);
    // wakeup
    wakeup(&os_env.ticks);
}
#include <environment.h>

void timer_tick() {
    spinlock_acquire(&env.ticks_lock);
    env.ticks++;
    spinlock_release(&env.ticks_lock);
}
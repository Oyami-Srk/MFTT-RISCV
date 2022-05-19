//
// Created by shiroko on 22-5-16.
//

#include <environment.h>
#include <lib/string.h>

env_t env;

void init_env() {
    memset(&env, 0, sizeof(env));
    spinlock_init(&env.ticks_lock);
    spinlock_init(&env.proc_lock);
    env.driver_list_head = (list_head_t)LIST_HEAD_INIT(env.driver_list_head);
    env.mem_sysmaps      = (list_head_t)LIST_HEAD_INIT(env.mem_sysmaps);
}

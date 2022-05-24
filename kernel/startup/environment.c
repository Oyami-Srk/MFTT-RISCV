//
// Created by shiroko on 22-5-16.
//

#include <environment.h>
#include <lib/string.h>

env_t os_env;

void init_env() {
    memset(&os_env, 0, sizeof(os_env));
    spinlock_init(&os_env.ticks_lock);
    spinlock_init(&os_env.proc_lock);
    os_env.driver_list_head = (list_head_t)LIST_HEAD_INIT(os_env.driver_list_head);
    os_env.mem_sysmaps      = (list_head_t)LIST_HEAD_INIT(os_env.mem_sysmaps);
}

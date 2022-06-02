//
// Created by shiroko on 22-5-16.
//

#include <environment.h>
#include <lib/string.h>

env_t os_env;

void init_env() {
    memset(&os_env, 0, sizeof(os_env));
    os_env.begin_gaurd = ENV_BEGIN_GUARD;
    os_env.end_gaurd   = ENV_END_GUARD;
    spinlock_init(&os_env.ticks_lock);
    spinlock_init(&os_env.proc_lock);
    os_env.driver_list_head =
        (list_head_t)LIST_HEAD_INIT(os_env.driver_list_head);
    os_env.mem_sysmaps = (list_head_t)LIST_HEAD_INIT(os_env.mem_sysmaps);
    os_env.procs       = (list_head_t)LIST_HEAD_INIT(os_env.procs);
    /* Boot stack:
     * boot_stack |  hart 1    | hart 0    | boot_sp
     */
    extern char boot_stack;
    extern char boot_sp;

    os_env.kernel_boot_stack     = &boot_stack;
    os_env.kernel_boot_stack_top = &boot_sp;
}

// Stack protector
uintptr_t __stack_chk_guard = 0x20010125beef5a5a;

__attribute__((noreturn)) void __stack_chk_fail(void) {
    kpanic("Stack overflow...");
    while (1)
        ;
}
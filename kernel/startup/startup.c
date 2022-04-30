#include <common/types.h>
#include <driver/SBI.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/sys/fdt.h>
#include <memory.h>
#include <proc.h>
#include <riscv.h>
#include <trap.h>

_Static_assert(sizeof(void *) == sizeof(uint64_t), "Target must be 64bit.");

env_t env;

volatile static int started = 0;

_Noreturn void kernel_main(uint64_t hartid, struct fdt_header *fdt_addr) {
    set_cpuid(hartid);
    if (cpuid() == 0) {
        memset(&env, 0, sizeof(env));
        spinlock_init(&env.ticks_lock);
        spinlock_init(&env.proc_lock);
        // Todo: init_console should use DTB resource instead SBI fucntion
        init_console();
        init_trap();
        kprintf("-*-*-*-*-*-*-*-*-*-*-*-*- My First Touch To RISC-V Starts "
                "Here... -*-*-*-*-*-*-*-*-*-*-*-*-\n");

        // Todo: Early memory allocator
        // 从Device Tree中保存一些我们需要的信息，
        // 其所占用的内存在初始化结束后将不被保证有效
        // 动态解析DTB能让内核的通用性加强。
        // 本内核应该不包含任何设备定义。
        // init_fdt会遍历DTB中所有的子节点并从已经添加的FDT Prober中选择相应的
        // prober函数来进行调用。
        init_fdt(fdt_addr);
        init_memory();

        proc_t *p1 = proc_alloc();
        proc_t *p2 = proc_alloc();
        proc_t *p3 = proc_alloc();

        started = 1;
    } else {
        // Salve cores
        init_trap();
        while (!started)
            ;
    }
    //    SBI_shutdown();
    while (1) {
        char *p = kmalloc(100);
        kprintf("Allocated: 0x%lx\n", p);
        kprintf("%d(%d).", cpuid(), env.ticks);
        kprintf("Freeing: 0x%lx\n", p);
        kfree(p);
        asm volatile("wfi");
    }
}
#include <common/types.h>
#include <driver/SBI.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/sys/fdt.h>
#include <memory.h>
#include <riscv.h>
#include <trap.h>

_Static_assert(sizeof(void *) == sizeof(uint64_t), "Target must be 64bit.");

env_t env;

void test() {
    volatile int i = 0;
    i++;
    i++;
    i++;
    i--;
    i++;
    while (1)
        ;
}

volatile static int started = 0;

_Noreturn void kernel_main(uint64_t hartid, struct fdt_header *fdt_addr) {
    set_cpuid(hartid);
    if (cpuid() == 0) {
        memset(&env, 0, sizeof(env));
        spinlock_init(&env.ticks_lock);
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

        // test proc
        if (cpuid() == 0) {
            pde_t root_pagedir = (pde_t)page_alloc(1, PAGE_TYPE_PGTBL);
            memset(root_pagedir, 0, PG_SIZE);
            pte_st *p               = (pte_st *)&root_pagedir[2];
            p->fields.V             = 1;
            p->fields.PhyPageNumber = 0x80000000 >> PG_SHIFT;
            p->fields.Type          = PTE_TYPE_RWX;
            p->fields.G             = 0;
            p->fields.U             = 1;

            env.proc[0].page_dir     = root_pagedir;
            env.proc[0].kernel_stack = page_alloc(1, PAGE_TYPE_INUSE);
            env.proc[0].kernel_sp    = env.proc[0].kernel_stack + PG_SIZE;
            env.proc[0].kernel_cpuid = cpuid();
            env.proc[0].user_pc      = (uint64_t)test;
            char *pg                 = page_alloc(1, PAGE_TYPE_INUSE);
            env.proc[0].trapframe.sp = pg + PG_SIZE;
            CSR_Write(sepc, env.proc[0].user_pc);
            user_ret(env.proc);
        }
        started = 1;
    } else {
        // Salve cores
        init_trap();
        while (!started)
            ;
    }
    //    SBI_shutdown();
    while (1) {
        asm volatile("wfi");
        //        kprintf("%d(%d).", cpuid(), env.ticks);
    }
}
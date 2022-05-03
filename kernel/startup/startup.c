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
#include <scheduler.h>
#include <trap.h>

#include <lib/elf.h>

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
        // manually setup process
        extern volatile char _prog1_bin_start_[];
        extern volatile char _prog2_bin_start_[];
        extern volatile char _prog3_bin_start_[];
        extern volatile char _prog1_bin_end_[];
        extern volatile char _prog2_bin_end_[];
        extern volatile char _prog3_bin_end_[];
        extern volatile char _prog_code_start_[];

        //        kprintf("Prog code start: 0x%lx.\n", _prog_code_start_);
        kprintf("Prog1 Start: 0x%lx.\n", _prog1_bin_start_);
        kprintf("Prog2 Start: 0x%lx.\n", _prog2_bin_start_);
        kprintf("Prog3 Start: 0x%lx.\n", _prog3_bin_start_);

        char *pg1_code = page_alloc(1, PAGE_TYPE_INUSE);
        char *pg2_code = page_alloc(1, PAGE_TYPE_INUSE);
        char *pg3_code = page_alloc(1, PAGE_TYPE_INUSE);
        memcpy(pg1_code, (char *)_prog1_bin_start_,
               _prog1_bin_end_ - _prog1_bin_start_);

        memcpy(pg2_code, (char *)_prog2_bin_start_,
               _prog2_bin_end_ - _prog2_bin_start_);

        memcpy(pg3_code, (char *)_prog3_bin_start_,
               _prog3_bin_end_ - _prog3_bin_start_);

        map_pages(p1->page_dir, 0, pg1_code, PG_SIZE, PTE_TYPE_RWX, true,
                  false);
        map_pages(p2->page_dir, 0, pg2_code, PG_SIZE, PTE_TYPE_RWX, true,
                  false);
        map_pages(p3->page_dir, 0, pg3_code, PG_SIZE, PTE_TYPE_RWX, true,
                  false);

        p1->trapframe.sp = (uintptr_t)(PG_SIZE);
        p2->trapframe.sp = (uintptr_t)(PG_SIZE);
        p3->trapframe.sp = (uintptr_t)(PG_SIZE);

        p1->status |= PROC_STATUS_READY;
        p2->status |= PROC_STATUS_READY;
        p3->status |= PROC_STATUS_READY;

        //        user_trap_return(p1);
        started = 1;
    } else {
        // Salve cores
        init_trap();
        while (!started)
            ;
    }

    while (1) {
        enable_trap();
        int result = scheduler();
        if (result == 1) {
            kprintf("Wait for interrupt.\n");
            asm volatile("wfi");
        } else {
            return_to_cpu_process();
        }
    }
}
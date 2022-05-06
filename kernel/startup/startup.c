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
        init_proc();

        /*
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

        typedef size_t (*elf_buffer_reader)(void *reader_data, uint64_t offset,
                                            char *target, size_t size);
        // test nested function, this is bad.
        size_t reader(void *data, uint64_t offset, char *target, size_t size) {
            memcpy(target, (char *)data + offset, size);
            return size;
        }

        elf_load_to_process(p1, reader, (void *)_prog1_bin_start_);
        elf_load_to_process(p2, reader, (void *)_prog2_bin_start_);
        elf_load_to_process(p3, reader, (void *)_prog3_bin_start_);

        // setup stack
        char *ps1 = page_alloc(1, PAGE_TYPE_INUSE | PAGE_TYPE_USER);
        char *ps2 = page_alloc(1, PAGE_TYPE_INUSE | PAGE_TYPE_USER);
        char *ps3 = page_alloc(1, PAGE_TYPE_INUSE | PAGE_TYPE_USER);

        map_pages(p1->page_dir, (void *)(0x80000000 - PG_SIZE), ps1, PG_SIZE,
                  PTE_TYPE_RW, true, false);
        map_pages(p2->page_dir, (void *)(0x80000000 - PG_SIZE), ps2, PG_SIZE,
                  PTE_TYPE_RW, true, false);
        map_pages(p3->page_dir, (void *)(0x80000000 - PG_SIZE), ps3, PG_SIZE,
                  PTE_TYPE_RW, true, false);

        p1->trapframe.sp = 0x80000000;
        p2->trapframe.sp = 0x80000000;
        p3->trapframe.sp = 0x80000000;

        p1->status |= PROC_STATUS_READY;
        //        p2->status |= PROC_STATUS_READY;
        //        p3->status |= PROC_STATUS_READY;
        */
        started = 1;
    } else {
        // Salve cores
        init_trap();
        while (!started)
            ;
    }

    // pre-CPU process runner
    while (1) {
        enable_trap();
        proc_t *proc = scheduler(&env.scheduler_data);
        if (proc) {
            assert(proc->lock.lock == true, "not holding the lock.");
            // change pre-CPU status
            if (mycpu()->proc && mycpu()->proc != proc) {
                spinlock_acquire(&mycpu()->proc->lock);
                mycpu()->proc->status &= ~PROC_STATUS_RUNNING;
                mycpu()->proc->status |= PROC_STATUS_READY;
                spinlock_release(&mycpu()->proc->lock);
            }
            mycpu()->proc = proc;
            // change status
            proc->status &= ~PROC_STATUS_READY;
            proc->status |= PROC_STATUS_RUNNING;
            // release lock
            spinlock_release(&proc->lock);
            return_to_cpu_process();
            // After process invoke yield, they returned here
            // without lock
            assert(myproc() == proc, "Current proc changed.");
        } else {
            kprintf("[%d]Wait for interrupt.\n", cpuid());
            enable_trap();
            asm volatile("wfi");
            kprintf("[%d]Recveivd interrupt.\n", cpuid());
            ;
        }
    }
}
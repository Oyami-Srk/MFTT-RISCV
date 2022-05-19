#include <common/types.h>
#include <dev/dev.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/sys/SBI.h>
#include <lib/sys/fdt.h>
#include <memory.h>
#include <proc.h>
#include <riscv.h>
#include <scheduler.h>
#include <trap.h>
#include <vfs.h>

_Static_assert(sizeof(void *) == sizeof(uint64_t), "Target must be 64bit.");

volatile static int started = 0;

_Noreturn void kernel_main(uint64_t hartid, struct fdt_header *fdt_addr) {
    set_cpuid(hartid);
    if (cpuid() == 0) {
        kprintf("-*-*-*-*-*-*-*-*-*-*-*-*- My First Touch To RISC-V Starts "
                "Here... -*-*-*-*-*-*-*-*-*-*-*-*-\n");

        init_env();
        init_trap();

        // 从Device Tree中保存一些我们需要的信息，
        // 其所占用的内存在初始化结束后将不被保证有效
        // 动态解析DTB能让内核的通用性加强。
        // 本内核应该不包含任何设备定义。
        // FIXME: FPIOA没有在DTB中，所以必须要在内核中包含设备定义
        // init_fdt会遍历DTB中所有的子节点并从已经添加的FDT Prober中选择相应的
        // prober函数来进行调用。
        init_fdt(fdt_addr);

        init_memory();
        init_plic();
        init_proc();
        init_vfs();

        int ret = 0;
        if ((ret = init_driver()) != 0)
            kpanic("Devices' driver cannot be initialized. Code: %d", ret);

        started = 1;
    } else {
        // Salve cores
        init_trap();
        while (!started)
            ;
    }

    assert(mycpu()->trap_off_depth == 0, "CPU enter scheduler with trap off.");
    // pre-CPU process runner
    while (1) {
        enable_trap();
        proc_t *proc = scheduler(&env.scheduler_data);
        if (proc) {
            assert(proc->lock.lock == true, "not holding the lock.");
            //            kprintf("CPU %d got PID %d.\n", cpuid(), proc->pid);
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
            mycpu()->proc = NULL;
            enable_trap();
            asm volatile("wfi");
            //            kprintf("[%d]Recveivd interrupt.\n", cpuid());
            ;
        }
    }
}
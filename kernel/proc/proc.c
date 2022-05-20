#include <environment.h>
#include <lib/bitset.h>
#include <lib/elf.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <proc.h>
#include <trap.h>

_Static_assert(sizeof(struct trap_context) == sizeof(uint64_t) * 31,
               "Trap context wrong.");

// for elf loader.
static size_t memory_reader(void *data, uint64_t offset, char *target,
                            size_t size) {
    memcpy(target, (char *)data + offset, size);
    return size;
}

void setup_init_process() {
    proc_t *proc = proc_alloc();
    assert(proc->pid == 1, "Init proc must be pid 1");
    extern volatile char _init_code_start[];
    extern volatile char _init_code_end[];

    elf_load_to_process(proc, memory_reader, (void *)_init_code_start);

    // setup stack
    char *process_stack = page_alloc(1, PAGE_TYPE_INUSE | PAGE_TYPE_USER);

    map_pages(proc->page_dir, (void *)(PROC_STACK_BASE - PG_SIZE),
              process_stack, PG_SIZE, PTE_TYPE_RW, true, false);

    uint64_t satp = ((uint64_t)proc->page_dir / PG_SIZE) |
                    ((uint64_t)PAGING_MODE_SV39 << 60);
    proc->page_csr = satp;

    proc->trapframe.sp = PROC_STACK_BASE;

    proc->status |= PROC_STATUS_READY;
}

void init_proc() {
    spinlock_acquire(&env.proc_lock);
    for (uint64_t i = 0; i < MAX_PROC; i++) {
        spinlock_init(&env.proc[i].lock);
    }
    spinlock_release(&env.proc_lock);
    // Make PID 0 as systask (never return to user space.)
    proc_alloc();
    // Setup init process as PID 1
    setup_init_process();
}

proc_t *proc_alloc() {
    // proc_t *proc = (proc_t *)kmalloc(sizeof(proc_t));
    // memset(proc, 0, sizeof(proc_t));
    proc_t *proc = NULL;
    spinlock_acquire(&env.proc_lock);
    uint64_t pid = (uint64_t)set_first_unset_bit(
        env.proc_bitmap,
        (MAX_PROC / BITS_PER_BITSET) + (MAX_PROC % BITS_PER_BITSET ? 1 : 0));
    if (unlikely(pid == 0xFFFFFFFFFFFFFFFF)) {
        spinlock_release(&env.proc_lock);
        // kfree(proc);
        return NULL;
    }
    proc = &env.proc[pid];
    env.proc_count++;
    spinlock_init(&proc->lock);
    spinlock_acquire(&proc->lock);
    spinlock_release(&env.proc_lock);
    proc->pid          = pid;
    proc->kernel_stack = page_alloc(1, PAGE_TYPE_SYSTEM);
    memset(proc->kernel_stack, 0, PG_SIZE);
    proc->kernel_sp = proc->kernel_stack + PG_SIZE;

    proc->page_dir = alloc_page_dir();
    if (!proc->page_dir) {
        spinlock_release(&proc->lock);
        return NULL;
    }
    proc->status = PROC_STATUS_NORMAL;

    proc->kernel_task_context.sp = (uintptr_t)proc->kernel_sp;
    proc->kernel_task_context.ra = (uintptr_t)user_trap_return;
    spinlock_release(&proc->lock);
    return proc;
}

void proc_free(proc_t *proc) {}

// Return current CPU process.
proc_t *myproc() {
    trap_push_off();
    proc_t *p = mycpu()->proc;
    trap_pop_off();
    return p;
}
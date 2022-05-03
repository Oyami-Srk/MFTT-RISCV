#include <environment.h>
#include <lib/bitset.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <proc.h>
#include <trap.h>

_Static_assert(sizeof(struct trap_context) == sizeof(uint64_t) * 31,
               "Trap context wrong.");

void init_proc() {
    spinlock_acquire(&env.proc_lock);
    for (uint64_t i = 0; i < MAX_PROC; i++) {
        spinlock_init(&env.proc[i].lock);
    }
    spinlock_release(&env.proc_lock);
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
    proc->page_dir  = (pde_t)page_alloc(1, PAGE_TYPE_PGTBL);
    memset(proc->page_dir, 0, PG_SIZE);
    // TODO: Set kernel pte
    pte_st *p               = (pte_st *)&proc->page_dir[2];
    p->fields.V             = 1;
    p->fields.PhyPageNumber = 0x80000000 >> PG_SHIFT;
    p->fields.Type          = PTE_TYPE_RWX;
    p->fields.G             = 1;
    p->fields.U             = 0;
    proc->status            = PROC_STATUS_NORMAL;

    proc->kernel_task_context.sp = (uintptr_t)proc->kernel_sp;
    proc->kernel_task_context.ra = (uintptr_t)user_trap_return;
    spinlock_release(&proc->lock);
    return proc;
}

void proc_free(proc_t *proc) {}

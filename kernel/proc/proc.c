#include <environment.h>
#include <lib/bitset.h>
#include <lib/elf.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <proc.h>
#include <stddef.h>
#include <trap.h>

_Static_assert(sizeof(struct trap_context) == sizeof(uint64_t) * 31,
               "Trap context wrong.");

static proc_t *proc_table[MAX_PROC] = {[0 ... MAX_PROC - 1] = NULL};

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

    assert(elf_load_to_process(proc, memory_reader, (void *)_init_code_start),
           "Failed to load init elf executable.");

    // setup init user stack
    size_t stack_pages = PG_ROUNDUP(PROC_STACK_SIZE) / PG_SIZE;
    char  *process_stack =
        page_alloc(stack_pages, PAGE_TYPE_INUSE | PAGE_TYPE_USER);
    if (!process_stack)
        kpanic("Cannot alloc %d page(s) to init process stack.", stack_pages);
    char *process_stack_top = (char *)(PROC_STACK_BASE - stack_pages * PG_SIZE);
    map_pages(proc->page_dir, (void *)(process_stack_top), process_stack,
              PROC_STACK_SIZE, PTE_TYPE_RW, true, false);
    flush_tlb_all();

    proc->trapframe.sp = (uintptr_t)PROC_STACK_BASE;
    proc->stack_bottom = (char *)PROC_STACK_BASE;
    proc->stack_top    = process_stack_top;
    strcpy(proc->name, "init");

    proc->status |= PROC_STATUS_READY;

    spinlock_release(&proc->lock);
}

void init_proc() {
    spinlock_acquire(&os_env.proc_lock);
    /*
    for (uint64_t i = 0; i < MAX_PROC; i++) {
        spinlock_init(&os_env.proc[i].lock);
    } */
    // leave proc 0 alone
    set_bit(os_env.proc_bitmap, 0);
    os_env.proc_count++;
    spinlock_release(&os_env.proc_lock);
    // Setup init process as PID 1
    setup_init_process();
}

// return process with locked
proc_t *proc_alloc() {
    // proc_t *proc = (proc_t *)kmalloc(sizeof(proc_t));
    // memset(proc, 0, sizeof(proc_t));
    proc_t *proc = NULL;
    spinlock_acquire(&os_env.proc_lock);
    uint64_t pid = (uint64_t)set_first_unset_bit(
        os_env.proc_bitmap,
        (MAX_PROC / BITS_PER_BITSET) + (MAX_PROC % BITS_PER_BITSET ? 1 : 0));
    if (unlikely(pid == 0xFFFFFFFFFFFFFFFF)) {
        spinlock_release(&os_env.proc_lock);
        // kfree(proc);
        return NULL;
    }
    // proc = &os_env.proc[pid];
    proc = (proc_t *)kmalloc(sizeof(proc_t));
    memset(proc, 0, sizeof(proc_t));
    spinlock_init(&proc->lock);
    os_env.proc_count++;
    list_add(&proc->proc_list, &os_env.procs);
    spinlock_acquire(&proc->lock);
    spinlock_release(&os_env.proc_lock);
    proc->pid       = pid;
    proc_table[pid] = proc;
    proc->children  = (list_head_t)LIST_HEAD_INIT(proc->children);

    proc->kernel_stack =
        page_alloc(PG_ROUNDUP(PROG_KSTACK_SIZE) / PG_SIZE, PAGE_TYPE_SYSTEM);
    memset(proc->kernel_stack, 0, PG_SIZE);
    proc->kernel_stack_top = proc->kernel_stack + PG_ROUNDUP(PROG_KSTACK_SIZE);
    proc->kernel_sp        = proc->kernel_stack_top;

    proc->page_dir = alloc_page_dir();
    if (!proc->page_dir) {
        spinlock_release(&proc->lock);
        return NULL; // TODO: clean up
    }
    // open 0,1,2 all to /dev/tty
    dentry_t *dentry      = vfs_get_dentry("/dev/tty", NULL);
    file_t   *file_output = vfs_open(dentry, O_WRONLY);
    file_t   *file_input  = vfs_open(dentry, O_RDONLY);
    proc->files[0]        = file_input;
    proc->files[1]        = file_output;
    proc->files[2]        = vfs_fdup(file_output);
    proc->status          = PROC_STATUS_NORMAL;
    proc->cwd             = vfs_get_root();

    proc->kernel_task_context.sp = (uintptr_t)proc->kernel_sp;
    proc->kernel_task_context.ra = (uintptr_t)user_trap_return;

    // setup satp
    uint64_t satp = ((uint64_t)proc->page_dir / PG_SIZE) |
                    ((uint64_t)PAGING_MODE_SV39 << 60);
    proc->page_csr = satp;

    return proc;
}

void proc_free(proc_t *proc) {
    // must hold lock
    assert(proc->lock.lock, "Proc free with unlocked.");
    // TODO: free resource. walk pde and decreate reference count
    // free file
    for (int i = 0; i < MAX_FILE_OPEN; i++) {
        if (proc->files[i])
            vfs_close(proc->files[i]);
    }
    // unmap all userspace
    pde_t pagedir = proc->page_dir;
    unmap_pages(pagedir, proc->prog_image_start,
                PG_ROUNDUP(proc->prog_size) / PG_SIZE, true);
    unmap_pages(pagedir, proc->stack_top,
                PG_ROUNDUP(proc->stack_bottom - (uintptr_t)proc->stack_top) /
                    PG_SIZE,
                true);
}

// Return current CPU process.
proc_t *myproc() {
    trap_push_off();
    proc_t *p = mycpu()->proc;
    trap_pop_off();
    return p;
}

// TODO: use rb tree to hold processes
proc_t *get_proc(pid_t pid) { return proc_table[pid]; }

void set_proc(pid_t pid, proc_t *proc) { proc_table[pid] = proc; }
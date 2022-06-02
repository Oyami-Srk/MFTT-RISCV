//
// Created by shiroko on 22-5-4.
//
#include <environment.h>
#include <lib/elf.h>
#include <lib/string.h>
#include <memory.h>
#include <proc.h>
#include <stddef.h>
#include <trap.h>
#include <vfs.h>

// for elf loader.
static size_t vfs_reader(void *data, uint64_t offset, char *target,
                         size_t size) {
    vfs_lseek(data, offset, SEEK_SET);
    return vfs_read(data, target, 0, size);
}

static int count_strs(const char **strs) {
    int r = 0;
    for (; strs[r] != NULL; r++)
        ;
    return r;
}

static inline char *get_offset_addr(char *base, char *addr, char *vbase) {
    return (vbase + (addr - base));
}

int do_execve(proc_t *old, dentry_t *cwd, const char *path, const char *argv[],
              const char *env[]) {
    // locate file
    dentry_t *dentry = vfs_get_dentry(path, cwd);
    if (!dentry)
        return -1; // no file
    if (dentry->d_type != D_TYPE_FILE)
        return -2; // not a file
    file_t *f = vfs_open(dentry, 0);
    if (!f)
        return -3; // cannot open file
    // setup new exec stack
    size_t stack_pages = PG_ROUNDUP(PROC_STACK_SIZE) / PG_SIZE;
    char  *process_stack =
        page_alloc(stack_pages, PAGE_TYPE_INUSE | PAGE_TYPE_USER);
    if (!process_stack)
        return -4; // cannot allocate stack
    char *stack_bottom = process_stack + PG_ROUNDUP(PROC_STACK_SIZE);
    char *sp           = stack_bottom;

    int argc = count_strs(argv);
    int envc = count_strs(env);

    // setup stack for envp
    for (int i = envc - 1; i >= 0; i--) {
        const char *e   = env[i];
        size_t      len = strlen(e);
        sp -= len + 1;
        memcpy(sp, e, len + 1);
    }
    char *head_of_envp = sp;
    sp = (char *)ROUNDDOWN_WITH(sizeof(uint64_t), sp); // align sp

    // setup stack for argv
    for (int i = argc - 1; i >= 0; i--) {
        const char *e   = argv[i];
        size_t      len = strlen(e);
        sp -= len + 1;
        memcpy(sp, e, len + 1);
    }
    char *head_of_argv = sp;
    sp = (char *)ROUNDDOWN_WITH(sizeof(uint64_t), sp); // align sp
    // TODO: setup stack for ELF aux

    // setup envp
    sp -= sizeof(char *) * (envc + 1);
    char **envp = (char **)sp;
    // envp[0]     = head_of_envp;
    envp[0] = get_offset_addr(process_stack, head_of_envp,
                              (char *)(PROC_STACK_BASE - PROC_STACK_SIZE));
    for (int i = 1; i <= envc; i++) {
        if (i == envc)
            envp[i] = NULL;
        else {
            const char *e   = env[i - 1];
            size_t      len = strlen(e);
            envp[i]         = envp[i - 1] + len + 1;
        }
    }
    size_t offset_envp = stack_bottom - sp;

    // setup argv
    sp -= sizeof(char *) * (argc + 1);
    char **argvp = (char **)sp;
    // argvp[0]     = head_of_argv;
    argvp[0] = get_offset_addr(process_stack, head_of_argv,
                               (char *)(PROC_STACK_BASE - PROC_STACK_SIZE));
    for (int i = 1; i <= argc; i++) {
        if (i == argc)
            argvp[i] = NULL;
        else {
            const char *e   = argv[i - 1];
            size_t      len = strlen(e);
            argvp[i]        = argvp[i - 1] + len + 1;
        }
    }
    size_t offset_argv = stack_bottom - sp;
    char  *envp_va     = (char *)(PROC_STACK_SIZE - offset_envp);
    char  *argv_va     = (char *)(PROC_STACK_SIZE - offset_argv);
    /* stack is like:
     * |  0x80000000  | <--- Stack base
     * +--------------+
     * | env strings  |
     * |    padding   |
     * | arg strings  |
     * |    padding   |
     * +--------------+
     * |  envp table  |
     * |  argv table  |
     * +--------------+ <--- we are here
     * |     envp     |
     * |     argv     |
     * |     argc     |
     * +--------------+
     * |      sp      | <--- user stack top
     */
    // push envp, argv, argc
    sp -= sizeof(uintptr_t);
    *((uintptr_t *)sp) = (uintptr_t)envp;
    sp -= sizeof(uintptr_t);
    *((uintptr_t *)sp) = (uintptr_t)argv;
    sp -= sizeof(uintptr_t);
    *((uintptr_t *)sp) = (uintptr_t)argc;

    // free old process's pages
    spinlock_acquire(&old->lock);
    // unmap all userspace
    pde_t pagedir = old->page_dir;
    unmap_pages(pagedir, old->prog_image_start,
                PG_ROUNDUP(old->prog_size) / PG_SIZE, true);
    unmap_pages(pagedir, old->stack_top,
                PG_ROUNDUP(old->stack_bottom - (uintptr_t)old->stack_top) /
                    PG_SIZE,
                true);
    spinlock_release(&old->lock);

    // load elf file
    bool ret = elf_load_to_process(old, vfs_reader, f);
    if (!ret) {
        // TODO: handle it
        kpanic("no way...todo here");
    }

    char *process_stack_top = (char *)(PROC_STACK_BASE - stack_pages * PG_SIZE);
    // load new stack
    map_pages(old->page_dir, (void *)(process_stack_top), process_stack,
              PROC_STACK_SIZE, PTE_TYPE_RW, true, false);

    old->trapframe.sp = ROUNDDOWN_WITH(
        sizeof(uintptr_t), (uintptr_t)(PROC_STACK_BASE - (stack_bottom - sp)));
    // These are riscv call convention
    old->trapframe.a1 = (uintptr_t)(argv_va);
    old->trapframe.a2 = (uintptr_t)(envp_va);

    old->stack_bottom = (char *)PROC_STACK_BASE;
    old->stack_top    = process_stack_top;
    flush_tlb_all();

    // close file
    vfs_close(f);

    // copy exec name
    strcpy(old->name, dentry->d_name);

    // let it go
    old->status = PROC_STATUS_READY | PROC_STATUS_NORMAL;

    return argc; // jump to switch with argc as a0
}

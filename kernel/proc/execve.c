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
    memcpy(target, (char *)data + offset, size);
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
    // TODO: a lock here?
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
    char *process_stack_top = (char *)(PROC_STACK_BASE - stack_pages * PG_SIZE);
    char *sp                = process_stack_top;

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

    size_t offset_sp = process_stack_top - sp;

    // free old process
    proc_free(old, true);

    // load new stack
    map_pages(old->page_dir, (void *)(process_stack_top), process_stack,
              PROC_STACK_SIZE, PTE_TYPE_RW, true, false);

    old->trapframe.sp = ROUNDDOWN_WITH(
        sizeof(uintptr_t), (uintptr_t)(PROC_STACK_BASE - offset_sp));
    old->trapframe.a1 = (uintptr_t)(PROC_STACK_SIZE - offset_sp);

    old->stack_bottom = (char *)PROC_STACK_BASE;
    old->stack_top    = process_stack_top;

    // load elf file
    bool ret = elf_load_to_process(old, vfs_reader, f);
    if (!ret) {
        // TODO: handle it
        kpanic("no way...todo here");
    }

    // close file
    vfs_close(f);

    // let it go
    old->status |= PROC_STATUS_READY;

    return argc; // jump to switch with argc as a0
}

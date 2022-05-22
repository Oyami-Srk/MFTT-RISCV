//
// Created by shiroko on 22-5-4.
//

#include <environment.h>
#include <lib/string.h>
#include <memory.h>
#include <proc.h>

int do_fork(proc_t *parent) {
    proc_t *child = proc_alloc();
    if (!child)
        return -1;
    // fork memory, TODO: but copy on write
    vm_copy(child->page_dir, parent->page_dir, parent->prog_image_start,
            parent->prog_break);

    // fork prog info
    child->parent           = parent;
    child->prog_brk_pg_end  = parent->prog_brk_pg_end;
    child->prog_image_start = parent->prog_image_start;
    child->prog_size        = parent->prog_size;
    child->prog_break       = parent->prog_break;

    // fork file table
    for (int fp = 3; fp < MAX_FILE_OPEN; fp++) {
        file_t *f = parent->files[fp];
        if (f) {
            file_t *nf = vfs_open(f->f_dentry, f->f_mode);
            if (!nf) {
                kpanic("failed to open file in fork. handle this.");
            }
            nf->f_offset     = f->f_offset;
            child->files[fp] = f;
        }
    }

    // fork others
    child->user_pc      = parent->user_pc;
    child->status       = parent->status;
    child->waiting_chan = parent->waiting_chan;
    memcpy(&child->trapframe, &parent->trapframe, sizeof(struct trap_context));

    spinlock_release(&child->lock);
}
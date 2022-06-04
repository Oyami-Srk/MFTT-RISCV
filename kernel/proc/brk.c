//
// Created by shiroko on 22-5-29.
//

#include <lib/string.h>
#include <memory.h>
#include <proc.h>

uintptr_t do_brk(proc_t *proc, uintptr_t addr) {
    assert(proc, "proc must valid.");
    char *current_brk = proc->prog_break;
    if (addr == 0)
        return (uintptr_t)current_brk;
    char *new_brk = (char *)addr;
    if (PG_ROUNDDOWN(current_brk) != PG_ROUNDDOWN(new_brk)) {
        size_t pgs =
            (PG_ROUNDDOWN(current_brk) - PG_ROUNDDOWN(new_brk)) / PG_SIZE;
        if (PG_ROUNDDOWN(new_brk) < PG_ROUNDDOWN(current_brk)) {
            // smaller, do free
            unmap_pages(proc->page_dir, (char *)(PG_ROUNDDOWN(current_brk)),
                        pgs, true);
        } else {
            char *new_pa = page_alloc(pgs, PAGE_TYPE_USER | PAGE_TYPE_INUSE);
            if (!new_pa)
                return -1;
            memset(new_pa, 0, pgs * PG_SIZE);
            if (map_pages(proc->page_dir,
                          (char *)(PG_ROUNDDOWN(current_brk) + PG_SIZE), new_pa,
                          PG_SIZE * pgs, PTE_TYPE_RW, true, false) != 0)
                return -1;
        }
    }
    proc->prog_break = new_brk;
    proc->prog_size  = proc->prog_break - proc->prog_image_start;
    return (uintptr_t)new_brk;
}

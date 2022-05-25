//
// Created by shiroko on 22-4-26.
//

#include "./utils.h"
#include <driver/console.h>
#include <environment.h>
#include <lib/linklist.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <memory.h>
#include <riscv.h>

/*
 * SV39: 三级页表，虚拟地址每级9位，一级共512个，刚好一页（4096Bytes）。
 * 根页表(一级页表)：一页1G，一张表映射512G（上半部256，下半部256，RISCV好怪哦）
 * 二级页表：一页2MB，一张表映射1G
 * 三级页表：一页4K，一张表映射2MB
 * 内核表用大页映射。
 */
#define PG_SIZE_LEVEL_1 (0x40000000)
#define PG_SIZE_LEVEL_2 (0x00200000)
#define PG_SIZE_LEVEL_3 (0x00001000) // equ to PG_SIZE

// extract the three 9-bit page table indices from a virtual address.
#define PX_MASK         0x1FF // 9 bits
#define PX_SHIFT(level) (PG_SHIFT + (9 * (level)))
#define PX(level, va)   ((((uint64_t)(va)) >> PX_SHIFT(level)) & PX_MASK)

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

_Static_assert(sizeof(pte_st) == sizeof(pte_t), "PTE Definination wrong.");

extern struct memory_info_t memory_info;

pte_t *walk_pages(pde_t page_dir, void *va, int alloc) {
    if ((uint64_t)va >= MAXVA)
        kpanic("Virtual address exceeded max virtual address.");
    for (int level = 2; level > 0; level--) {
        pte_st *pte = (pte_st *)&page_dir[PX(level, va)];
        if (pte->fields.V) {
            page_dir =
                (pde_t)(((uint64_t)pte->fields.PhyPageNumber) << PG_SHIFT);
        } else {
            if (!alloc ||
                (page_dir = (pde_t)page_alloc(1, PAGE_TYPE_PGTBL)) == NULL)
                return NULL;
            memset(page_dir, 0, PG_SIZE);
            pte->fields.PhyPageNumber = (uint64_t)page_dir >> PG_SHIFT;
            pte->fields.V             = 1;
        }
    }
    return &page_dir[PX(0, va)];
}

int map_pages(pde_t page_dir, void *va, void *pa, uint64_t size, int type,
              bool user, bool global) {
    void   *a, *last;
    pte_st *pte;
    a    = (void *)PG_ROUNDDOWN((uint64_t)va);
    last = (void *)PG_ROUNDDOWN((uint64_t)va + size - 1);
    // TODO: large page map
    for (;;) {
        if ((pte = (pte_st *)walk_pages(page_dir, a, 1)) == NULL)
            return -1;
        if (pte->fields.V) {
            kprintf("[MEM] Paging remap for VA 0x%lx => PA 0x%lx. pte addr: "
                    "0x%lx.\n",
                    a, pa, pte);
            return -2;
        }
        pte->fields.PhyPageNumber = (uint64_t)pa >> PG_SHIFT;
        pte->fields.Type          = type;
        pte->fields.V             = 1;
        pte->fields.U             = user;
        pte->fields.G             = global;
        // *pte = PA2PTE(pa) | perm | PTE_V;
        // increase_page_ref(&memory_info, pa);
        if (a == last)
            break;
        a += PG_SIZE;
        pa += PG_SIZE;
    }
    return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void unmap_pages(pde_t page_dir, void *va, size_t size, int do_free) {
    void   *a;
    pte_st *pte;

    if (((uint64_t)va % PG_SIZE) != 0)
        kpanic("vmunmap: not aligned");

    for (a = va; a < va + size * PG_SIZE; a += PG_SIZE) {
        if ((pte = (pte_st *)walk_pages(page_dir, a, 0)) == 0)
            kpanic("vmunmap: walk");
        if (pte->fields.V == 0)
            kpanic("vmunmap: not mapped");
        if (pte->fields.Type == 0)
            kpanic("vmunmap: not a leaf");
        char *pa = (char *)((uint64_t)pte->fields.PhyPageNumber << PG_SHIFT);
        kprintf("unmap 0x%lx => 0x%lx, pte addr: 0x%lx.\n", a, pa, pte);
        int ref = decrease_page_ref(&memory_info, pa);
        if (do_free && ref == 0) {
            page_free(pa, 1);
        }
        pte->raw = 0;
    }
}

void init_paging(void *init_start, void *init_end) {
    os_env.kernel_pagedir = (pde_t)page_alloc(1, PAGE_TYPE_PGTBL);
    memset(os_env.kernel_pagedir, 0, PG_SIZE);
    size_t init_pg_start = ((uint64_t)init_start) >> 30;
    size_t init_pg_end   = ((uint64_t)init_end) >> 30;
    // in position map
#if FALSE
    map_pages(env.kernel_pagedir, init_start, init_start, init_end - init_start,
              PTE_TYPE_RWX, false, true);
#else
    pte_st *p               = (pte_st *)&os_env.kernel_pagedir[2];
    p->fields.V             = 1;
    p->fields.PhyPageNumber = 0x80000000 >> PG_SHIFT;
    p->fields.Type          = PTE_TYPE_RWX;
    p->fields.G             = 1;
    p->fields.U             = 0;
#endif
    uint64_t satp = ((uint64_t)os_env.kernel_pagedir / PG_SIZE) |
                    ((uint64_t)PAGING_MODE_SV39 << 60);
    os_env.kernel_satp = satp;
    CSR_Write(satp, satp);
    flush_tlb_all();
}

// TODO: Currently sysmap only used in init. No lock here.
int mem_sysmap(void *va, void *pa, size_t size, int type) {
    struct mem_sysmap *sysmap =
        (struct mem_sysmap *)kmalloc(sizeof(struct mem_sysmap));
    memset(sysmap, 0, sizeof(struct mem_sysmap));
    sysmap->va   = va;
    sysmap->pa   = pa;
    sysmap->type = type;
    sysmap->size = size;
    if (map_pages(os_env.kernel_pagedir, va, pa, size, type, false, true) !=
        0) {
        return -1;
    }
    list_add(&sysmap->list, &os_env.mem_sysmaps);
    return 0;
}

int mem_sysunmap(void *va) {
    // TODO: impl this.
    return 0;
}

pde_t alloc_page_dir() {
    pde_t pgdir = (pde_t)page_alloc(1, PAGE_TYPE_PGTBL);
    if (!pgdir)
        return NULL;
    memset(pgdir, 0, PG_SIZE);
    // Setup big kernel pte from 0x80000000 ~ 0xC0000000
    pte_st *p               = (pte_st *)&pgdir[2];
    p->fields.V             = 1;
    p->fields.PhyPageNumber = 0x80000000 >> PG_SHIFT;
    p->fields.Type          = PTE_TYPE_RWX;
    p->fields.G             = 1;
    p->fields.U             = 0;
    // Setup sysmap
    list_foreach_entry(&os_env.mem_sysmaps, struct mem_sysmap, list, sysmap) {
        // TODO: check result.
        map_pages(pgdir, sysmap->va, sysmap->pa, sysmap->size, sysmap->type,
                  false, true);
    }
    return pgdir;
}

int vm_copy(pde_t dst, pde_t src, char *start, char *end) {
    // debug
    kprintf("do vm copy at va 0x%lx ~ 0x%lx.\n", start, end);

    char   *a;
    char   *va     = (char *)PG_ROUNDDOWN(start);
    char   *va_end = (char *)PG_ROUNDUP(end);
    pte_st *pte;

    for (a = va; a < va_end; a += PG_SIZE) {
        if ((pte = (pte_st *)walk_pages(src, a, 0)) == 0)
            kpanic("vm_copy cannot walk pages");
        if (pte->fields.V == 0)
            kpanic("vm_copy: source not mapped");
        if (pte->fields.Type == 0)
            kpanic("vm_copy: not a leaf");
        // do copy map
        char *pa = (char *)((uintptr_t)(pte->fields.PhyPageNumber << PG_SHIFT));
        pte_st *cpte;
        if ((cpte = (pte_st *)walk_pages(dst, a, 1)) == NULL)
            kpanic("Cannot walk child pte.");
        if (cpte->fields.V) {
            kpanic("Child pte remap while copy.");
        }
        cpte->fields.PhyPageNumber = pte->fields.PhyPageNumber;
        cpte->fields.V             = 1;
        cpte->fields.U             = pte->fields.U;
        cpte->fields.G             = pte->fields.G;
        assert(cpte->fields.U == 1, "Must be user area for vm_copy");
        increase_page_ref(&memory_info, pa);
        // set read-only for CoW
        uint8_t type = pte->fields.Type;
        type &= ~PTE_TYPE_BIT_W; // clear write flag
        cpte->fields.Type = pte->fields.Type = type;
    }
}

int do_pagefault(char *caused_va, pde_t pde, bool from_kernel) {
    proc_t *proc = myproc();
    assert(proc->page_dir == pde,
           "PDE not identical to currently holding process.");
    if ((uintptr_t)caused_va >= KERN_BASE) {
        kprintf("not use page fault.");
        return -3;
    }
    kprintf("DO PF for 0x%lx, pde: 0x%lx.\n", caused_va, pde);
    pte_st *pte = (pte_st *)walk_pages(pde, caused_va, 0);
    char   *pa  = (char *)((uintptr_t)(pte->fields.PhyPageNumber << PG_SHIFT));
    if (!(pa >= memory_info.usable_memory_start &&
          pa < memory_info.usable_memory_end)) {
        kprintf("not use page fault.");
        return -3;
    }
    if (!pte)
        return -1;
    // kprintf("PF pa: 0x%lx, reference count: %d.\n", pa,
    //        get_page_reference(&memory_info, pa));
    if (from_kernel && (CSR_Read(sstatus) & SSTATUS_SUM) == 0) {
        CSR_RWOR(sstatus, SSTATUS_SUM);
        return 0;
    }
    uint8_t type = pte->fields.Type;
    if (type & PTE_TYPE_BIT_W) {
        kprintf("PF invailed.\n");
        return -4;
    }
    if (get_page_reference(&memory_info, pa) == 1) {
        // just change type
        type |= PTE_TYPE_BIT_W;
        pte->fields.Type = type;
    } else {
        // do copy
        char *new_pa = page_alloc(1, PAGE_TYPE_INUSE | PAGE_TYPE_USER);
        if (!new_pa)
            return -2;
        memcpy(new_pa, pa, PG_SIZE);
        pte->fields.PhyPageNumber = ((uintptr_t)new_pa >> PG_SHIFT);
        decrease_page_ref(&memory_info, pa);
        type |= PTE_TYPE_BIT_W;
        pte->fields.Type = type;
    }
    flush_tlb_all();
    return 0;
}
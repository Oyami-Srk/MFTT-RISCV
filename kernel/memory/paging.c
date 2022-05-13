//
// Created by shiroko on 22-4-26.
//

#include <driver/console.h>
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
        if (pte->fields.V)
            kpanic("remap");
        pte->fields.PhyPageNumber = (uint64_t)pa >> PG_SHIFT;
        pte->fields.Type          = type;
        pte->fields.V             = 1;
        pte->fields.U             = user;
        pte->fields.G             = global;
        //        *pte = PA2PTE(pa) | perm | PTE_V;
        if (a == last)
            break;
        a += PG_SIZE;
        pa += PG_SIZE;
    }
    return 0;
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
        if (do_free) {
            char *pa =
                (char *)((uint64_t)pte->fields.PhyPageNumber << PG_SHIFT);
            page_free(pa, 1);
        }
        pte->raw = 0;
    }
}

// TODO: move to environment
pde_t root_pagedir;
void  init_paging(void *init_start, void *init_end) {
     kprintf("Left KBytes: %ld.\n", memory_available() / 1024);
     root_pagedir = (pde_t)page_alloc(1, PAGE_TYPE_PGTBL);
     memset(root_pagedir, 0, PG_SIZE);
     size_t init_pg_start = ((uint64_t)init_start) >> 30;
     size_t init_pg_end   = ((uint64_t)init_end) >> 30;
     // in position map
#if FALSE
    map_pages(root_pagedir, init_start, init_start, init_end - init_start,
              PTE_TYPE_RWX, false, true);
#else
    pte_st *p               = (pte_st *)&root_pagedir[2];
    p->fields.V             = 1;
    p->fields.PhyPageNumber = 0x80000000 >> PG_SHIFT;
    p->fields.Type          = PTE_TYPE_RWX;
    p->fields.G             = 1;
    p->fields.U             = 0;
#endif
    uint64_t satp =
        ((uint64_t)root_pagedir / PG_SIZE) | ((uint64_t)PAGING_MODE_SV39 << 60);
    CSR_Write(satp, satp);
    flush_tlb_all();
}

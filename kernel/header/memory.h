//
// Created by shiroko on 22-4-20.
//

#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <common/types.h>
#include <lib/bitset.h>

#define PAGING_MODE_BARE 0
#define PAGING_MODE_SV32 1
#define PAGING_MODE_SV39 8

#define PTE_TYPE_DIRENTRY 0 // Point to next level of page table
#define PTE_TYPE_RO       1
#define PTE_TYPE_RSV1     2
#define PTE_TYPE_RW       3
#define PTE_TYPE_XO       4
#define PTE_TYPE_RX       5
#define PTE_TYPE_RSV2     6
#define PTE_TYPE_RWX      7

typedef uint64_t pte_t;
typedef pte_t   *pde_t;
typedef union {
    struct {
        uint8_t  V : 1;
        uint8_t  Type : 3;
        uint8_t  U : 1;
        uint8_t  G : 1;
        uint8_t  A : 1;
        uint8_t  D : 1;
        uint8_t  Reserved1 : 2;
        uint64_t PhyPageNumber : 38;
        uint16_t Reserved2 : 16;
    } __attribute__((packed)) fields;
    pte_t raw;
} pte_st;

#define KERN_BASE 0x80200000 // Must keep same as kernel.ld

#define PG_SIZE         4096
#define PG_SHIFT        12
#define PG_ROUNDUP(sz)  (((sz) + PG_SIZE - 1) & ~(PG_SIZE - 1))
#define PG_ROUNDDOWN(a) (((a)) & ~(PG_SIZE - 1))

#define PAGE_TYPE_USABLE   0x001
#define PAGE_TYPE_RESERVED 0x002
#define PAGE_TYPE_SYSTEM   0x004
#define PAGE_TYPE_HARDWARE 0x008
#define PAGE_TYPE_FREE     0x010
#define PAGE_TYPE_INUSE    0x020
#define PAGE_TYPE_PGTBL    0x040
#define PAGE_TYPE_USER     0x080
#define PAGE_TYPE_POOL     0x100

struct page_info {
    uint16_t type;
    uint16_t reference; // 引用计数，最大65535。应该够用吧.
};

#define ROUNDUP_WITH(s, a)   ((((uint64_t)(a)) + (s)-1) & ~((s)-1))
#define ROUNDDOWN_WITH(s, a) ((((uint64_t)(a))) & ~((s)-1))

struct _block_list {
    struct _block_list *prev;
    struct _block_list *next;
};

typedef struct _block_list block_list;

struct memory_info_t {
#define MAX_BUDDY_ORDER 11 // max block is 4MB
    void  *memory_start;
    void  *memory_end;
    void  *usable_memory_start;
    void  *usable_memory_end; // Buddy map located on the end of memory
    size_t page_count;

    bitset *buddy_map[MAX_BUDDY_ORDER]; // Bitmap for each order of buddy
    struct page_info *pages_info;
    block_list       *free_list[MAX_BUDDY_ORDER];
    size_t            free_count[MAX_BUDDY_ORDER];
};

#define GET_PAGE_BY_ID(mem, id)                                                \
    ((void *)(((mem).memory_start + ((id)*PG_SIZE))))
#define GET_ID_BY_PAGE(mem, page)                                              \
    (((void *)(page) - ((mem).memory_start)) / PG_SIZE)

void   init_memory();
size_t memory_available();

char *page_alloc(size_t pages, int attr);
int   page_free(char *p, size_t pages);

void  kfree(void *p);
char *kmalloc(size_t size);

#endif // __MEMORY_H__
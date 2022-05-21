#include "./utils.h"
#include <common/types.h>
#include <driver/console.h>
#include <lib/stdlib.h>
#include <memory.h>

extern struct memory_info_t memory_info; // in memory.c

static block_list *remove_from_free_list(block_list *p, int order) {
    uintptr_t end = (uintptr_t)p + PG_SIZE * (1 << order);
    assert(end > (uintptr_t)memory_info.usable_memory_start &&
               end < (uintptr_t)memory_info.usable_memory_end,
           "Block execeeded while remove.");
    if (p->prev) {
        p->prev->next = p->next;
        if (p->next)
            p->next->prev = p->prev;
    } else {
        if (memory_info.free_list[order] != p)
            kpanic("Block has no prev but not the first.");
        memory_info.free_list[order] = p->next;
        if (p->next)
            p->next->prev = NULL;
    }
    p->next = NULL;
    p->prev = NULL;
    return p;
}

static block_list *attach_to_free_list(block_list *p, int order) {
    uintptr_t end = (uintptr_t)p + PG_SIZE * (1 << order);
    assert(end > (uintptr_t)memory_info.usable_memory_start &&
               end < (uintptr_t)memory_info.usable_memory_end,
           "Block execeeded while attach.");
    // must attach a single node
    p->next = memory_info.free_list[order];
    if (p->next)
        p->next->prev = p;
    p->prev                      = NULL;
    memory_info.free_list[order] = p;
    return p;
}

static inline int xor_buddy_map(char *p, int order) {
    size_t page_idx = (p - memory_info.usable_memory_start) / PG_SIZE;
    xor_bit(memory_info.buddy_map[order], page_idx >> (order + 1), 1);
    return check_bit(memory_info.buddy_map[order], page_idx >> (order + 1));
}

static char *allocate_pages_of_power_2(int order, int attr) {
    if (order >= MAX_BUDDY_ORDER)
        return NULL;
    char *block = NULL;
    if (memory_info.free_count[order] == 0) {
        block = allocate_pages_of_power_2(order + 1, 0);
        if (block == NULL)
            return NULL;
        attach_to_free_list((block_list *)block, order);
        memory_info.free_count[order]++;
        /* printf("= PUT %x into order %d free list =\n", block); */
        block += ((1 << order) * PG_SIZE);
        xor_buddy_map(block, order); // higher half is returned
    } else {
        block =
            (char *)remove_from_free_list(memory_info.free_list[order], order);
        memory_info.free_count[order]--;
        xor_buddy_map(block, order);
    }
    if (block != NULL) {
        for (int i = 0; i < (1 << order); i++) {
            struct page_info *pg = &memory_info.pages_info[GET_ID_BY_PAGE(
                memory_info, (block + i * PG_SIZE))];
            pg->type             = attr;
            pg->reference        = 1;
        }
    }
    return block;
}

static int free_pages_of_power_2(char *p, int order) {
    if (p == NULL)
        return 1; // free a NULL block
    if (!(p >= memory_info.usable_memory_start &&
          p <= memory_info.usable_memory_end))
        return 2; // free a block not managed by us
    int buddy_bit = xor_buddy_map(p, order);
    if (buddy_bit == 0 && order + 1 < MAX_BUDDY_ORDER) {
        char  *buddy    = NULL;
        size_t page_idx = (p - memory_info.usable_memory_start) / PG_SIZE;
        size_t buddy_even_page_idx = (page_idx >> (order + 1)) << (order + 1);
        if (page_idx == buddy_even_page_idx)
            buddy = p + (1 << order) * PG_SIZE;
        else
            buddy = p - (1 << order) * PG_SIZE;
        // printf("= REMOVE %x from order %d free list =\n", buddy, order);
        remove_from_free_list((block_list *)buddy, order);
        memory_info.free_count[order]--;
        if (buddy < p)
            free_pages_of_power_2(buddy, order + 1);
        else
            free_pages_of_power_2(p, order + 1);
    } else {
        attach_to_free_list((block_list *)p, order);
        clear_page_info(&memory_info, p, 1 << order,
                        PAGE_TYPE_USABLE | PAGE_TYPE_FREE);
        memory_info.free_count[order]++;
    }
    return 0;
}

void print_free_info() {
    kprintf("[MEM] free blocks count is\n[MEM] ");
    for (int i = 0; i < MAX_BUDDY_ORDER; i++)
        kprintf("%04d, ", 1 << i);
    kprintf("\n[MEM] ");
    for (int i = 0; i < MAX_BUDDY_ORDER; i++)
        kprintf("%04d, ", memory_info.free_count[i]);
    kprintf("\n[MEM ]");
    for (int i = 0; i < MAX_BUDDY_ORDER; i++)
        kprintf("0x%x,", memory_info.free_list[i]);
    kprintf("\n");
}

char *page_alloc(size_t pages, int attr) {
    int order = trailing_zero(round_up_power_2(pages));
    spinlock_acquire(&memory_info.lock);
    char *r = allocate_pages_of_power_2(order, attr);
    spinlock_release(&memory_info.lock);
    return r;
}
int page_free(char *p, size_t pages) {
    spinlock_acquire(&memory_info.lock);
    int r = free_pages_of_power_2((char *)p,
                                  trailing_zero(round_up_power_2(pages)));
    spinlock_release(&memory_info.lock);
    return r;
}

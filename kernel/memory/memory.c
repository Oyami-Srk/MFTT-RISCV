#include <driver/console.h>
#include <lib/bitset.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <memory.h>

struct memory_info_t memory_info;

// kmem.c
extern void attach_to_memory_pool(char *mem, size_t sz);
// paging.c
extern void init_paging(void *init_start, void *init_end);

void init_memory() {
    spinlock_init(&memory_info.lock);
    spinlock_acquire(&memory_info.lock);
    kprintf("[MEM] Init memory From 0x%lx - 0x%lx\n", memory_info.memory_start,
            memory_info.memory_end);
    size_t pg_count =
        (memory_info.memory_end - memory_info.memory_start) / PG_SIZE;
    size_t pg_info_size = pg_count * sizeof(struct page_info);
    size_t buddy_table_size =
        ((1 << (MAX_BUDDY_ORDER)) - 1) * pg_count / (1 << MAX_BUDDY_ORDER);
    size_t max_block_size = (1 << (MAX_BUDDY_ORDER - 1)) *
                            PG_SIZE; // minimal allocate unit is a page.
    memory_info.page_count   = pg_count;
    memory_info.buddy_map[0] = (bitset_t *)ROUNDDOWN_WITH(
        0x10, (memory_info.memory_end - buddy_table_size));
    memory_info.pages_info = (struct page_info *)ROUNDDOWN_WITH(
        0x10, ((char *)(memory_info.buddy_map[0]) - pg_info_size));
    memory_info.usable_memory_end =
        (void *)ROUNDDOWN_WITH(PG_SIZE, memory_info.pages_info);

    memset(memory_info.usable_memory_end, 0,
           memory_info.memory_end - memory_info.usable_memory_end);
    kprintf("[MEM] Buddy allocator's map at 0x%lx, size %ld bytes.\n",
            memory_info.buddy_map[0], buddy_table_size);
    kprintf("[MEM] Pages info at 0x%lx, size %ld bytes.\n",
            memory_info.pages_info, pg_info_size);
    kprintf("[MEM] Usable memory: 0x%lx - 0x%lx. System occupied memory size: "
            "%ld KBytes.\n",
            memory_info.usable_memory_start, memory_info.usable_memory_end,
            (memory_info.memory_end - memory_info.usable_memory_end) / 1024);

    memory_info.free_count[0] = 0;
    memory_info.free_list[0]  = (block_list *)NULL;
    for (int i = 0; i < MAX_BUDDY_ORDER - 1; i++) {
        memory_info.buddy_map[i + 1] = (bitset_t *)((memory_info.buddy_map[i]) +
                                                    (pg_count >> (i + 1)) / 8);
        memory_info.free_list[i + 1] = (block_list *)NULL;
        memory_info.free_count[i + 1] = 0;
    }
#if 0
    void *pg            = NULL;
    int   current_order = MAX_BUDDY_ORDER;
    for (pg = memory_info.usable_memory_start;
         pg < memory_info.usable_memory_end;) {
        // Assure pg could contains current order size.
        size_t current_size = (1 << (current_order - 1)) * PG_SIZE;
        do {
            if (pg + current_size < memory_info.usable_memory_end) {
                break;
            }
            current_order--;
            current_size = (1 << (current_order - 1)) * PG_SIZE;
            if (current_order <= 0) {
                current_size = 0;
                break;
            }
        } while (current_order > 0);
        if (!current_size)
            break;

        block_list *current = (block_list *)pg;
        current->next       = memory_info.free_list[current_order - 1];
        current->prev       = NULL;
        if (current->next)
            current->next->prev = current;
        memory_info.free_list[current_order - 1] = current;
        memory_info.free_count[current_order - 1]++;
        pg += current_size;
    }
#else
    char *pg = NULL;
    for (pg = memory_info.usable_memory_start;
         pg < memory_info.usable_memory_end - max_block_size;
         pg += max_block_size) {
        block_list *current = (block_list *)pg;
        current->next       = memory_info.free_list[MAX_BUDDY_ORDER - 1];
        current->prev       = NULL;
        if (current->next)
            current->next->prev = current;
        memory_info.free_list[MAX_BUDDY_ORDER - 1] = current;
        memory_info.free_count[MAX_BUDDY_ORDER - 1]++;
    }
    kprintf("[MEM] Waste: %ld Kbytes.\n",
            (memory_info.usable_memory_end - (pg - max_block_size)) / 1024);
#endif
    // setup system page info
    for (size_t i = 0; i < pg_count; i++) {
        char *page = GET_PAGE_BY_ID(memory_info, i);
        if (page >= memory_info.usable_memory_start &&
            page < memory_info.usable_memory_end)
            memory_info.pages_info[i].type = PAGE_TYPE_FREE | PAGE_TYPE_USABLE;
        else
            memory_info.pages_info[i].type = PAGE_TYPE_INUSE | PAGE_TYPE_SYSTEM;
    }
    kprintf("[MEM] Finish Initialization.\n");
    // TODO: init memory pool with wasted memory
    spinlock_release(&memory_info.lock);
    char  *pool_addr = (char *)PG_ROUNDUP(pg);
    size_t pool_sz   = memory_info.usable_memory_end - pool_addr;
    attach_to_memory_pool(pool_addr, pool_sz);

    kprintf("[MEM] Setup SV39 MMU. In position mapping for kernel.\n");
    init_paging(memory_info.memory_start, memory_info.memory_end);
}

// Note: kmem pool not inside
size_t memory_available() {
    size_t sz = 0;
    for (int order = 0; order < MAX_BUDDY_ORDER; order++) {
        size_t current_size = (1 << (order)) * PG_SIZE;
        sz += memory_info.free_count[order] * current_size;
    }
    return sz;
}

#include <lib/sys/fdt.h>

extern volatile char _KERN_END[];
#define KERN_END (((void *)(_KERN_END)))
extern volatile char _KERN_BASE[];
#define KERN_BASE (((void *)(_KERN_BASE)))

static int memory_fdt_prober(uint32_t version, const char *node_name,
                             const char *begin, uint32_t addr_cells,
                             uint32_t size_cells, const char *strings) {
    kprintf("[FDT] FDT Prober for Memory with node name: %s\n", node_name);
    const char *p = begin;
    uint32_t    tag;
    int         depth     = 1;
    int         discoverd = 0;
    while ((tag = FDT_OFFSET_32(p, 0)) != FDT_END_NODE && depth != 0) {
        p += 4;
        switch (tag) {
        case FDT_BEGIN_NODE: {
            const char *str = p;
            p               = PALIGN(p + strlen(str) + 1, 4);
            assert(str != NULL, "Node str cannot be null.");
            depth++;
            break;
        }
        case FDT_END_NODE:
            depth--;
        case FDT_NOP:
            break;
        case FDT_PROP: {
            uint32_t size = FDT_OFFSET_32(p, 0);
            p += 4;
            const char *str = strings + FDT_OFFSET_32(p, 0);
            p += 4;
            if (version < 16 && size >= 8)
                p = PALIGN(p, 8);
            const char *p_value = p;

            p = PALIGN(p + size, 4);
            // kprintf("// %s (size:%d) \n", str, size);
            if (strcmp(str, "device_type") == 0) {
                assert(strcmp(p_value, "memory") == 0,
                       "Device type must be \"memory\" for memory.");
            } else if (strcmp(str, "reg") == 0) {
                uint64_t mem_addr = 0x0;
                uint64_t mem_size = 0x0;
                fdt32_t *addr_base =
                    (fdt32_t *)(p_value + sizeof(fdt32_t) * (addr_cells - 1));
                fdt32_t *size_base =
                    (fdt32_t *)(p_value + sizeof(fdt32_t) *
                                              (addr_cells + size_cells - 1));
                mem_addr = CPU_TO_FDT32(*addr_base);
                if (addr_cells >= 2)
                    mem_addr |= CPU_TO_FDT32(*(addr_base - 1)) << 32;
                mem_size = CPU_TO_FDT32(*size_base);
                // for k210 port, TODO: enable all memory
                mem_size = 0x600000;
                if (size_cells >= 2)
                    mem_size |= CPU_TO_FDT32(*(size_base - 1)) << 32;
                kprintf("[FDT] Detected Memory @ 0x%x with Size 0x%x bytes.\n",
                        mem_addr, mem_size);
                if (mem_size != 0 && discoverd == 0) {
                    // TODO: pick large size memory if needed.
                    memory_info.memory_start = (void *)mem_addr;
                    memory_info.memory_end =
                        (void *)((char *)mem_addr + mem_size);
                    if (memory_info.memory_end >= (char *)KERN_END &&
                        memory_info.memory_start <= (char *)KERN_BASE) {
                        memory_info.usable_memory_start =
                            (void *)ROUNDUP_WITH(PG_SIZE, KERN_END);
                        discoverd = 1;
                    }
                }
            }
            break;
        }
        default:
            kpanic("Unknown FDT Tag Note: 0x%08x.\n", tag);
            break;
        }
    }
    assert(discoverd, "[FDT] Memory undetected.");
    return (int)(p - begin);
}

static fdt_prober prober = {.name = "memory", .prober = memory_fdt_prober};

ADD_FDT_PROBER(prober);
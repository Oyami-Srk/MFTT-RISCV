//
// Created by shiroko on 22-5-18.
//

#include "./virtio.h"
#include <common/types.h>
#include <dev/dev.h>
#include <driver/console.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <memory.h>
#include <proc.h>
#include <riscv.h>
#include <vfs.h>

static struct virtio_mmio_info_t {
    char  *pa;
    size_t size;
    int    interrupt, interrupt_parent;
} virtio_mmio_info[VIRTIO_MMIO_MAX_BUS];
static int virtio_mmio_bus_count = 0;

struct virtio_mmio_setup_t {
    int                    device;
    virtio_setup_handler_t handler;
    list_head_t            list;
};

static list_head_t setup_handlers = (list_head_t)LIST_HEAD_INIT(setup_handlers);

// TODO: impl vfs for mmio.
int virtio_mmio_write(file_t *file, const char *buffer, size_t offset,
                      size_t len) {}

int virtio_mmio_read(file_t *file, char *buffer, size_t offset, size_t len) {}

static inode_ops_t inode_ops = {
    .link = NULL, .lookup = NULL, .mkdir = NULL, .rmdir = NULL, .unlink = NULL};

static file_ops_t file_ops = {
    .flush  = NULL,
    .mmap   = NULL,
    .munmap = NULL,
    .write  = virtio_mmio_write,
    .read   = virtio_mmio_read,
    .open   = NULL,
    .close  = NULL,
    .seek   = NULL,
};

int virtio_register_device(int device, virtio_setup_handler_t setup_handler) {
    list_foreach_entry(&setup_handlers, struct virtio_mmio_setup_t, list,
                       setup) {
        if (setup->device == device)
            return -1;
    }
    struct virtio_mmio_setup_t *setup = (struct virtio_mmio_setup_t *)kmalloc(
        sizeof(struct virtio_mmio_setup_t));
    memset(setup, 0, sizeof(struct virtio_mmio_setup_t));
    setup->device  = device;
    setup->handler = setup_handler;

    list_add(&setup->list, &setup_handlers);
}

int virtio_queue_desc_alloc(virtio_mmio_queue_t *queue) {
    int idx = (int)set_first_unset_bit(
        queue->desc_used_map,
        BITSET_ARRAY_SIZE_FOR(VIRTIO_MMIO_QUEUE_NUM_VALUE));
    if (idx >= VIRTIO_MMIO_QUEUE_NUM_VALUE)
        return -1;
    return idx;
}

void virtio_queue_desc_free(virtio_mmio_queue_t *queue, int idx) {
    // FIXME: may be a panic?
    if (idx >= VIRTIO_MMIO_QUEUE_NUM_VALUE) {
        kprintf("[MMIO] MMIO Queue descriptor id execeed. Weired.\n");
        return;
    }
    if (check_bit(queue->desc_used_map, idx) == 0) {
        kprintf("[MMIO] MMIO Queue descriptor id already be free. Weired\n");
        return;
    }
    queue->io_ring->desc[idx].addr = 0;
    clear_bit(queue->desc_used_map, idx);
    // application may sleep on this
    wakeup(queue->desc_used_map);
}

int virtio_queue_desc_alloc_some(virtio_mmio_queue_t *queue, int num,
                                 int *idxs) {
    if (num >= VIRTIO_MMIO_QUEUE_NUM_VALUE) {
        kprintf("[MMIO] MMIO Requires too many descriptos. (got %d expected "
                "smaller than %d.)\n",
                num, VIRTIO_MMIO_QUEUE_NUM_VALUE);
        return -1;
    }
    for (int i = 0; i < num; i++) {
        idxs[i] = virtio_queue_desc_alloc(queue);
        if (idxs[i] < 0) {
            for (int j = 0; j < i; j++)
                virtio_queue_desc_free(queue, idxs[j]);
            return -1;
        }
    }
    return 0;
}

void virtio_queue_desc_free_chain(virtio_mmio_queue_t *queue, int idx) {
    for (;;) {
        virtio_queue_desc_free(queue, idx);
        if (queue->io_ring->desc[idx].flags & VIRTIO_MMIO_QUEUE_DESC_F_NEXT)
            // have next
            idx = queue->io_ring->desc[idx].next;
        else
            break;
    }
}

int init_virtio_mmio(dev_driver_t *drv) {
    kprintf("[MMIO] Virtio.MMIO Start initialize.\n");
    for (int i = 0; i < virtio_mmio_bus_count; i++) {
        struct virtio_mmio_info_t *info       = &virtio_mmio_info[i];
        char                      *vbase_addr = HARDWARE_VBASE + info->pa;

        if (mem_sysmap(vbase_addr, info->pa, info->size, PTE_TYPE_RW) != 0) {
            kprintf("[MMIO] Cannot map MMIO Bus from 0x%lx to 0x%lx.\n",
                    info->pa, vbase_addr);
            continue;
        }
        flush_tlb_all();
        if (MEM_IO_READ(uint32_t, vbase_addr + VIRTIO_MMIO_MAGIC_VALUE) !=
                VIRTIO_MMIO_MAGIC ||
            MEM_IO_READ(uint32_t, vbase_addr + VIRTIO_MMIO_VENDOR_ID) !=
                VIRTIO_MMIO_VENDOR_QEMU ||
            MEM_IO_READ(uint32_t, vbase_addr + VIRTIO_MMIO_VERSION) != 1) {
            kprintf("[MMIO] MMIO Bus at 0x%lx is invalid.\n", info->pa);
            continue;
        }
        int device = MEM_IO_READ(uint32_t, vbase_addr + VIRTIO_MMIO_DEVICE_ID);
        if (device)
            list_foreach_entry(&setup_handlers, struct virtio_mmio_setup_t,
                               list, setup) {
                if (device == setup->device)
                    setup->handler(vbase_addr, info->interrupt,
                                   info->interrupt_parent);
            }
    }
    // Once finished setup, free all recorded setup handler
    kprintf("[MMIO] All Virtio.MMIO Device setup complete.\n");
    return 0;
}

dev_driver_t virtio_mmio = {
    .name             = "virtio_mmio",
    .init             = init_virtio_mmio,
    .loading_sequence = 2,
    .dev_id           = 9,
    .major_ver        = 0,
    .minor_ver        = 1,
    .private_data     = NULL,
    .list             = LIST_HEAD_INIT(virtio_mmio.list),
};

ADD_DEV_DRIVER(virtio_mmio);

// virtio-mmio fdt prober
#include <lib/sys/fdt.h>
static int mmio_fdt_prober(uint32_t version, const char *node_name,
                           const char *begin, uint32_t addr_cells,
                           uint32_t size_cells, const char *strings) {
    kprintf("[FDT] FDT Prober for virtio_mmio with node name: %s\n", node_name);
    const char *p = begin;
    uint32_t    tag;
    int         depth     = 1;
    int         discoverd = 0;
    uint8_t     interrupts, interrupt_parent;
    char       *base_addr;
    size_t      mem_size;

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
            if (strcmp(str, "reg") == 0) {
                uint64_t addr = 0x0;
                fdt32_t *addr_base =
                    (fdt32_t *)(p_value + sizeof(fdt32_t) * (addr_cells - 1));
                fdt32_t *size_base =
                    (fdt32_t *)(p_value + sizeof(fdt32_t) *
                                              (addr_cells + size_cells - 1));
                addr = CPU_TO_FDT32(*addr_base);
                if (addr_cells >= 2)
                    addr |= CPU_TO_FDT32(*(addr_base - 1)) << 32;
                mem_size = CPU_TO_FDT32(*size_base);
                if (size_cells >= 2)
                    mem_size |= CPU_TO_FDT32(*(size_base - 1));
                if (addr) {
                    base_addr = (char *)addr;
                    discoverd++;
                }
            } else if (strcmp(str, "interrupts") == 0) {
                fdt32_t *base = (fdt32_t *)(p_value);
                interrupts    = CPU_TO_FDT32(*base) & 0xFF;
                if (interrupts)
                    discoverd++;
            } else if (strcmp(str, "interrupt-parent") == 0) {
                fdt32_t *base    = (fdt32_t *)(p_value);
                interrupt_parent = CPU_TO_FDT32(*base) & 0xFF;
                if (interrupt_parent)
                    discoverd++;
            }
            break;
        }
        default:
            kpanic("Unknown FDT Tag Note: 0x%08x.\n", tag);
            break;
        }
    }
    assert(discoverd == 3,
           "Insufficient information provided for virtio mmio.");
    kprintf("[FDT] virtio.mmio discovered at 0x%lx with IRQ %d and Parent's "
            "IRQ %d.\n",
            base_addr, interrupts, interrupt_parent);
    virtio_mmio_info[virtio_mmio_bus_count] =
        (struct virtio_mmio_info_t){.pa               = base_addr,
                                    .size             = mem_size,
                                    .interrupt        = interrupts,
                                    .interrupt_parent = interrupt_parent};
    virtio_mmio_bus_count++;
    return (int)(p - begin);
}

static fdt_prober prober = {.name = "virtio_mmio", .prober = mmio_fdt_prober};

ADD_FDT_PROBER(prober);

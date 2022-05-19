//
// Created by shiroko on 22-5-18.
//

#include "./virtio.h"
#include <common/types.h>
#include <dev.h>
#include <driver/console.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <memory.h>
#include <riscv.h>
#include <vfs.h>

static struct virtio_mmio_info_t {
    char  *pa;
    size_t size;
    int    interrupt, interrupt_parent;
} virtio_mmio_info[VIRTIO_MMIO_MAX_BUS];
static int virtio_mmio_bus_count = 0;

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

int init_virtio_mmio(dev_driver_t *drv) {
    kprintf("[MMIO] Virtio.MMIO Start initialize.\n");
    for (int i = 0; i < virtio_mmio_bus_count; i++) {
        struct virtio_mmio_info_t *info       = &virtio_mmio_info[i];
        char                      *vbase_addr = HARDWARE_VBASE + info->pa;

        if (mem_sysmap(vbase_addr, info->pa, info->size, PTE_TYPE_RW) != 0) {
            kprintf("[MMIO] Cannot map MMIO Bus from 0x%lx to 0x%lx.\n",
                    info->pa, vbase_addr);
            continue;
        } else
            kprintf("[MMIO] Mapped MMIO Bus from 0x%lx to 0x%lx.\n", info->pa,
                    vbase_addr);
        flush_tlb_all();
        if (vbase_addr == 0xD0001000) {
            char c = *vbase_addr;
            kprintf("::%c.", c);
        }
    }
    return 0;
}

dev_driver_t virtio_mmio = {
    .name             = "virtio_mmio",
    .init             = init_virtio_mmio,
    .loading_sequence = 0,
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

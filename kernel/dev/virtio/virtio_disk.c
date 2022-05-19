//
// Created by shiroko on 22-5-18.
//

// TODO: Support virtio.pci
#include "./virtio.h"
#include <common/types.h>
#include <dev/dev.h>
#include <driver/console.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/sys/spinlock.h>
#include <memory.h>

typedef struct {
    uint32_t type; // 0: Read; 1: Write; 4: Flush; 11: Discard; 13: Write zeros
    uint32_t rsvd;
    uint64_t sector;
    uint8_t *data;   // data size must be multiple of 512
    uint8_t  status; // 0: OK; 1: Error; 2: Unsupported
} virtio_block_request_t;

static struct {
    char                     *io_addr;
    bool                      initialized;
    spinlock_t                lock;
    virtio_mmio_queue_desc_t *queue_desc; // PhyAddr
} virtio_disk = {.initialized = false, .io_addr = NULL};

void virtio_disk_setup(char *ioaddr, int interrupt, int interrupt_parent) {
    kprintf("[DISK] Setup Virtio Disk at MMIO Bus 0x%lx.\n", ioaddr);
    if (virtio_disk.initialized) {
        kprintf("[DISK] Currently only support one virtio disk. But it's not "
                "hard to support many.\n");
        return;
    }
    virtio_disk.initialized = true;
    spinlock_init(&virtio_disk.lock);
    spinlock_acquire(&virtio_disk.lock);
    virtio_disk.io_addr = ioaddr;
    // Setup MMIO
    MEM_IO_WRITE(uint32_t, ioaddr + VIRTIO_MMIO_STATUS,
                 MEM_IO_READ(uint32_t, ioaddr + VIRTIO_MMIO_STATUS) |
                     (VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER));

    // get device supported features
    uint32_t features =
        MEM_IO_READ(uint32_t, ioaddr + VIRTIO_MMIO_DEVICE_FEATURES);
    // disable what we don't need
    features &= ~(VIRTIO_BLK_F_RO | VIRTIO_BLK_F_SCSI | VIRTIO_BLK_F_MQ |
                  VIRTIO_BLK_F_CONFIG_WCE | VIRTIO_F_ANY_LAYOUT |
                  VIRTIO_RING_F_EVENT_IDX | VIRTIO_RING_F_INDIRECT_DESC);
    MEM_IO_WRITE(uint32_t, ioaddr + VIRTIO_MMIO_DRIVER_FEATURES, features);

    // Features OK
    MEM_IO_WRITE(uint32_t, ioaddr + VIRTIO_MMIO_STATUS,
                 MEM_IO_READ(uint32_t, ioaddr + VIRTIO_MMIO_STATUS) |
                     (VIRTIO_CONFIG_S_FEATURES_OK));

    // Set guest page size
    MEM_IO_WRITE(uint32_t, ioaddr + VIRTIO_MMIO_GUEST_PAGE_SIZE, PG_SIZE);

    // Initialize MMIO Queue
    MEM_IO_WRITE(uint32_t, ioaddr + VIRTIO_MMIO_QUEUE_SEL, 0);
    // Detected disk queue
    uint32_t max_num =
        MEM_IO_READ(uint32_t, ioaddr + VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (unlikely(max_num == 0)) {
        kprintf("[DISK] Err: Virtio Disk queue size is 0.\n");
        goto failed;
    }
    if (unlikely(max_num < VIRTIO_MMIO_QUEUE_NUM_VALUE)) {
        kprintf("[DISK] Err: Virtio Disk queue size too small (got %d expected "
                "at least %d).\n",
                max_num, VIRTIO_MMIO_QUEUE_NUM_VALUE);
        goto failed;
    }
    MEM_IO_WRITE(uint32_t, ioaddr + VIRTIO_MMIO_QUEUE_NUM,
                 VIRTIO_MMIO_QUEUE_NUM_VALUE);
    virtio_disk.queue = page_alloc()

        // Driver OK
        MEM_IO_WRITE(uint32_t, ioaddr + VIRTIO_MMIO_STATUS,
                     MEM_IO_READ(uint32_t, ioaddr + VIRTIO_MMIO_STATUS) |
                         (VIRTIO_CONFIG_S_DRIVER_OK));

    spinlock_release(&virtio_disk.lock);
    return;
failed:
    virtio_disk.initialized = false;
    spinlock_release(&virtio_disk.lock);
}

int init_virtio_mmio_disk(dev_driver_t *drv) {
    kprintf("[DISK] Register Virtio.MMIO disk setup handler.\n");
    virtio_register_device(VIRTIO_DEVICE_DISK, virtio_disk_setup);
    return 0;
}

dev_driver_t virtio_mmio_disk = {
    .name             = "virtio_mmio_disk",
    .init             = init_virtio_mmio_disk,
    .loading_sequence = 1, // load before actually virtio mmio setup.
    .dev_id           = 9 + 1,
    .major_ver        = 0,
    .minor_ver        = 1,
    .private_data     = NULL,
    .list             = LIST_HEAD_INIT(virtio_mmio_disk.list),
};

ADD_DEV_DRIVER(virtio_mmio_disk);
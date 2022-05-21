//
// Created by shiroko on 22-5-18.
//

// TODO: Support virtio.pci
#include <types.h>
#include "./virtio.h"
#include <dev/dev.h>
#include <driver/console.h>
#include <lib/bitset.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/sys/sleeplock.h>
#include <lib/sys/spinlock.h>
#include <memory.h>
#include <proc.h>
#include <trap.h>

#define VIRTIO_BLK_T_IN  0 // read the disk
#define VIRTIO_BLK_T_OUT 1 // write the disk
#define SECTOR_SIZE      512

typedef struct {
    uint32_t type; // 0: Read; 1: Write; 4: Flush; 11: Discard; 13: Write zeros
    uint32_t rsvd;
    uint64_t sector;
    uint8_t *data;   // data size must be multiple of 512
    uint8_t  status; // 0: OK; 1: Error; 2: Unsupported
} virtio_block_request_t;

static struct {
    char               *io_addr;
    bool                initialized;
    virtio_mmio_queue_t queue;
    struct {
        // TODO: link to buffered io
        char   *addr;
        size_t  size;
        uint8_t status;
        uint8_t ok;
    } trace[VIRTIO_MMIO_QUEUE_NUM_VALUE];
    spinlock_t lock;
} virtio_disk = {.initialized = false};

static int virtio_disk_interrupt_handler(void);
static int virtio_disk_read(file_t *file, char *buffer, size_t offset,
                            size_t len);
static int virtio_disk_write(file_t *file, const char *buffer, size_t offset,
                             size_t len);
static int virtio_disk_rw(uint64_t sector, char *buf, size_t count, int func);
static int virtio_disk_rw_lba(size_t offset, char *buf, size_t len, int func);

static inode_ops_t inode_ops = {
    .link = NULL, .lookup = NULL, .mkdir = NULL, .rmdir = NULL, .unlink = NULL};

static file_ops_t file_ops = {
    .flush  = NULL,
    .mmap   = NULL,
    .munmap = NULL,
    .write  = virtio_disk_write,
    .read   = virtio_disk_read,
    .open   = NULL,
    .close  = NULL,
    .seek   = NULL,
};

void virtio_disk_setup(char *ioaddr, int interrupt, int interrupt_parent) {
    kprintf("[DISK] Setup Virtio Disk at MMIO Bus 0x%lx.\n", ioaddr);
    if (virtio_disk.initialized) {
        kprintf("[DISK] Currently only support one virtio disk. But it's not "
                "hard to support many.\n");
        return;
    }
    memset(&virtio_disk, 0, sizeof(virtio_disk));
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
    // setup io ring
    size_t              io_ring_size = PG_ROUNDUP(sizeof(virtio_mmio_ring_t));
    virtio_mmio_ring_t *io_ring      = (virtio_mmio_ring_t *)page_alloc(
             io_ring_size / PG_SIZE, PAGE_TYPE_HARDWARE);
    memset(io_ring, 0, io_ring_size);

    MEM_IO_WRITE(uint32_t, ioaddr + VIRTIO_MMIO_QUEUE_PFN,
                 ((uintptr_t)io_ring) >> PG_SHIFT);

    virtio_disk.queue.io_ring  = io_ring;
    virtio_disk.queue.used_idx = 0;

    // setup interrupt
    if (interrupt_try_reg(interrupt, virtio_disk_interrupt_handler) != 0)
        kpanic("Disk interrupt already been registered.");

    plic_register_irq(interrupt);

    // Driver OK
    MEM_IO_WRITE(uint32_t, ioaddr + VIRTIO_MMIO_STATUS,
                 MEM_IO_READ(uint32_t, ioaddr + VIRTIO_MMIO_STATUS) |
                     (VIRTIO_CONFIG_S_DRIVER_OK));

    // Setup vfs
    inode_t *inode   = vfs_alloc_inode(NULL);
    inode->i_f_op    = &file_ops;
    inode->i_op      = &inode_ops;
    inode->i_dev[0]  = DEV_VIRTIO_DISK;
    inode->i_dev[1]  = 1;
    inode->i_fs_data = (void *)virtio_disk_rw_lba;

    vfs_link_inode(inode, vfs_get_dentry("/dev", NULL), "raw_vda");

    spinlock_release(&virtio_disk.lock);
    kprintf("[DISK] Setup Virtio Disk complete.\n");
    return;
failed:
    virtio_disk.initialized = false;
    spinlock_release(&virtio_disk.lock);
}

// func: 0 is read, 1 is write. buf must be pa and SECTOR_SIZE multiple
int virtio_disk_rw(uint64_t sector, char *buf, size_t count, int func) {
    spinlock_acquire(&virtio_disk.lock);
    // legacy block device requires three descs:
    // Type, data, result
    int idx[3];
    for (;;) {
        if (virtio_queue_desc_alloc_some(&virtio_disk.queue, 3, idx) == 0)
            break;
        sleep(&virtio_disk.queue.desc_used_map, &virtio_disk.lock);
    }
    // setup descs
    struct {
        uint32_t type;
        uint32_t rsvd;
        uint64_t sector;
    } buf0 = {.type   = func == 0 ? VIRTIO_BLK_T_IN : VIRTIO_BLK_T_OUT,
              .rsvd   = 0,
              .sector = sector};
    // buf0 in kstack is directly mapped
    virtio_mmio_queue_desc_t *desc_table = virtio_disk.queue.io_ring->desc;
    desc_table[idx[0]].addr              = (uintptr_t)&buf0;
    desc_table[idx[0]].length            = sizeof(buf0);
    desc_table[idx[0]].flags             = VIRTIO_MMIO_QUEUE_DESC_F_NEXT;
    desc_table[idx[0]].next              = idx[1];

    desc_table[idx[1]].addr   = (uintptr_t)buf;
    desc_table[idx[1]].length = count * SECTOR_SIZE;
    if (func == 0)
        // we read, device write only
        desc_table[idx[1]].flags = VIRTIO_MMIO_QUEUE_DESC_F_WRITE_ONLY;
    else
        // we write, device read only
        desc_table[idx[1]].flags = 0;
    desc_table[idx[1]].next = idx[2];
    desc_table[idx[1]].flags |= VIRTIO_MMIO_QUEUE_DESC_F_NEXT;

    desc_table[idx[2]].addr   = (uintptr_t)(&virtio_disk.trace[idx[0]].status);
    desc_table[idx[2]].length = 1;
    desc_table[idx[2]].flags  = VIRTIO_MMIO_QUEUE_DESC_F_WRITE_ONLY;
    desc_table[idx[2]].next   = 0;

    virtio_disk.trace[idx[0]].status = 0;
    virtio_disk.trace[idx[0]].ok     = 0;
    virtio_disk.trace[idx[0]].addr   = (char *)sector;
    virtio_disk.trace[idx[0]].size   = count * SECTOR_SIZE;

    // put into avail ring
    virtio_mmio_queue_avail_t *avail = &virtio_disk.queue.io_ring->avail;
    avail->ring[avail->idx % VIRTIO_MMIO_QUEUE_NUM_VALUE] = idx[0];
    __sync_synchronize(); // make sure compiler not reorder.
    avail->idx++;

    // issue the queue
    MEM_IO_WRITE(uint32_t, virtio_disk.io_addr + VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    // wait the interrupt
    while (virtio_disk.trace[idx[0]].ok == 0) {
        sleep(&virtio_disk.trace[idx[0]], &virtio_disk.lock);
    }

    // got data
    virtio_queue_desc_free_chain(&virtio_disk.queue, idx[0]);

    spinlock_release(&virtio_disk.lock);
    return count;
}

static int virtio_disk_rw_lba(size_t offset, char *buf, size_t len, int func) {
    size_t rounded_size  = 0;
    size_t sector        = offset / SECTOR_SIZE;
    size_t sector_offset = offset % SECTOR_SIZE;
    size_t elen          = len + sector_offset;
    char  *kbuf;
    if (elen < PG_SIZE)
        kbuf =
            (char *)kmalloc((rounded_size = ROUNDUP_WITH(SECTOR_SIZE, elen)));
    else
        kbuf = (char *)kmalloc((rounded_size = PG_ROUNDUP(elen)));
    memset(kbuf, 0, rounded_size);
    virtio_disk_rw(sector, kbuf, rounded_size / SECTOR_SIZE, func);
    memcpy(buf, kbuf + sector_offset, len);
    return len;
}

static int virtio_disk_read(file_t *file, char *buffer, size_t offset,
                            size_t len) {
    size_t rounded_size  = 0;
    size_t sector        = offset / SECTOR_SIZE;
    size_t sector_offset = offset % SECTOR_SIZE;
    size_t elen          = len + sector_offset;
    char  *kbuf;
    if (elen < PG_SIZE)
        kbuf =
            (char *)kmalloc((rounded_size = ROUNDUP_WITH(SECTOR_SIZE, elen)));
    else
        kbuf = (char *)kmalloc((rounded_size = PG_ROUNDUP(elen)));
    memset(kbuf, 0, rounded_size);
    virtio_disk_rw(sector, kbuf, rounded_size / SECTOR_SIZE, 0);
    memcpy(buffer, kbuf + sector_offset, len);
    return len;
}

static int virtio_disk_write(file_t *file, const char *buffer, size_t offset,
                             size_t len) {
    size_t rounded_size  = 0;
    size_t sector        = offset / SECTOR_SIZE;
    size_t sector_offset = offset % SECTOR_SIZE;
    size_t elen          = len + sector_offset;
    char  *kbuf;
    if (elen < PG_SIZE)
        kbuf =
            (char *)kmalloc((rounded_size = ROUNDUP_WITH(SECTOR_SIZE, elen)));
    else
        kbuf = (char *)kmalloc((rounded_size = PG_ROUNDUP(elen)));
    memset(kbuf, 0, rounded_size);
    virtio_disk_rw(sector, kbuf, rounded_size / SECTOR_SIZE, 0);
    memcpy(kbuf + sector_offset, buffer, len);
    virtio_disk_rw(sector, kbuf, rounded_size / SECTOR_SIZE, 1);
    return len;
}

// Interrupt handler
static int virtio_disk_interrupt_handler(void) {
    spinlock_acquire(&virtio_disk.lock);
    uint32_t                 *used_idx = &virtio_disk.queue.used_idx;
    virtio_mmio_queue_used_t *used     = &virtio_disk.queue.io_ring->used;

    while ((*used_idx % VIRTIO_MMIO_QUEUE_NUM_VALUE) !=
           (used->idx % VIRTIO_MMIO_QUEUE_NUM_VALUE)) {
        int idx = used->ring[*used_idx].id;
        if (virtio_disk.trace[idx].status != 0)
            kpanic("Virtio disk status non-zero.");
        virtio_disk.trace[idx].ok = 1;
        wakeup(&virtio_disk.trace[idx]);
        *used_idx = (*used_idx + 1) % VIRTIO_MMIO_QUEUE_NUM_VALUE;
    }
    spinlock_release(&virtio_disk.lock);
}

// Device setup for register.

int init_virtio_mmio_disk(dev_driver_t *drv) {
    kprintf("[DISK] Register Virtio.MMIO disk setup handler.\n");
    virtio_register_device(VIRTIO_DEVICE_DISK, virtio_disk_setup);
    return 0;
}

dev_driver_t virtio_mmio_disk = {
    .name             = "virtio_mmio_disk_register",
    .init             = init_virtio_mmio_disk,
    .loading_sequence = 1, // load before actually virtio mmio setup.
    .dev_id           = DEV_VIRTIO_DISK,
    .major_ver        = 0,
    .minor_ver        = 1,
    .private_data     = NULL,
    .list             = LIST_HEAD_INIT(virtio_mmio_disk.list),
};

ADD_DEV_DRIVER(virtio_mmio_disk);
//
// Created by shiroko on 22-5-18.
//

#ifndef __VIRTIO_VIRTIO_H__
#define __VIRTIO_VIRTIO_H__

#include <common/types.h>

#define VIRTIO_MMIO_MAX_BUS     32
#define VIRTIO_MMIO_MAGIC       0x74726976
#define VIRTIO_MMIO_VENDOR_QEMU 0x554D4551

// https://docs.oasis-open.org/virtio/virtio/v1.2/csd01/virtio-v1.2-csd01.html#x1-2160005
// MAX_DEVICE_ID is 42 for current version.
#define VIRTIO_DEVICE_NET  1
#define VIRTIO_DEVICE_DISK 2

// clang-format off
// Fields defininations
// from qemu virtio_mmio.h
#define VIRTIO_MMIO_MAGIC_VALUE      0x000 // 0x74726976
#define VIRTIO_MMIO_VERSION          0x004 // version; 1 is legacy
#define VIRTIO_MMIO_DEVICE_ID        0x008 // device type; 1 is net, 2 is disk
#define VIRTIO_MMIO_VENDOR_ID        0x00c // 0x554d4551, QEMU
#define VIRTIO_MMIO_DEVICE_FEATURES  0x010
#define VIRTIO_MMIO_DRIVER_FEATURES  0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE  0x028 // page size for PFN, write-only
#define VIRTIO_MMIO_QUEUE_SEL        0x030 // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX    0x034 // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM        0x038 // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_ALIGN      0x03c // used ring alignment, write-only
#define VIRTIO_MMIO_QUEUE_PFN        0x040 // physical page number for queue, read/write
#define VIRTIO_MMIO_QUEUE_READY      0x044 // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY     0x050 // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060 // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK    0x064 // write-only
#define VIRTIO_MMIO_STATUS           0x070 // read/write

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 0x1
#define VIRTIO_CONFIG_S_DRIVER      0x2
#define VIRTIO_CONFIG_S_DRIVER_OK   0x4
#define VIRTIO_CONFIG_S_FEATURES_OK 0x8

// device feature bits
#define VIRTIO_BLK_F_RO             (1<<5)  /* Disk is read-only */
#define VIRTIO_BLK_F_SCSI           (1<<7)  /* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE     (1<<11) /* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             (1<<12) /* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT         (1<<27)
#define VIRTIO_RING_F_INDIRECT_DESC (1<<28)
#define VIRTIO_RING_F_EVENT_IDX     (1<<29)

#define VIRTIO_MMIO_QUEUE_NUM_VALUE 8       // Must be power of 2

// clang-format on

// Virtio MMIO Queue ring
// Version 1.0 (legacy)
typedef struct {
    uint64_t addr; // Physical Addr
    uint32_t length;
#define VIRTIO_MMIO_QUEUE_DESC_F_NEXT       1
#define VIRTIO_MMIO_QUEUE_DESC_F_WRITE_ONLY 2
#define VIRTIO_MMIO_QUEUE_DESC_F_INDIRECT   4
    uint16_t flags;
    uint16_t next;
} virtio_mmio_queue_desc_t;

typedef struct {
#define VIRTIO_MMIO_QUEUE_AVAIL_F_NO_INTERRUPT 1
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_MMIO_QUEUE_NUM_VALUE];
    uint16_t used_event; // Only if virtio_f_event_idx
} virtio_mmio_queue_avail_t;

typedef struct {
#define VIRTIO_MMIO_QUEUE_USED_F_NO_NOTIFY 1
    uint16_t flags;
    uint16_t idx;
    struct __virtio_mmio_queue_used_elem_t {
        uint32_t id;
        uint32_t len;
    } ring[VIRTIO_MMIO_QUEUE_NUM_VALUE];
    uint16_t avail_event; // Only if ...
} virtio_mmio_queue_used_t;

// Functions for register virtio device
typedef void (*virtio_setup_handler_t)(char *addr, int interrupt,
                                       int interrupt_parent);
int virtio_register_device(int device, virtio_setup_handler_t setup_handler);

#endif // __VIRTIO_VIRTIO_H__
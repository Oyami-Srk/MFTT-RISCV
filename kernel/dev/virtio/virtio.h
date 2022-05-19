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

// Fields defininations
// from qemu virtio_mmio.h
#define VIRTIO_MMIO_MAGIC_VALUE     0x000 // 0x74726976
#define VIRTIO_MMIO_VERSION         0x004 // version; 1 is legacy
#define VIRTIO_MMIO_DEVICE_ID       0x008 // device type; 1 is net, 2 is disk
#define VIRTIO_MMIO_VENDOR_ID       0x00c // 0x554d4551, QEMU
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028 // page size for PFN, write-only
#define VIRTIO_MMIO_QUEUE_SEL       0x030 // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034 // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM       0x038 // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_ALIGN     0x03c // used ring alignment, write-only
#define VIRTIO_MMIO_QUEUE_PFN                                                  \
    0x040 // physical page number for queue, read/write
#define VIRTIO_MMIO_QUEUE_READY      0x044 // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY     0x050 // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060 // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK    0x064 // write-only
#define VIRTIO_MMIO_STATUS           0x070 // read/write

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER      2
#define VIRTIO_CONFIG_S_DRIVER_OK   4
#define VIRTIO_CONFIG_S_FEATURES_OK 8

// device feature bits
#define VIRTIO_BLK_F_RO             5  /* Disk is read-only */
#define VIRTIO_BLK_F_SCSI           7  /* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE     11 /* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12 /* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29

// Functions for register virtio device
typedef void (*virtio_setup_handler_t)(char *addr, int interrupt,
                                       int interrupt_parent);
int virtio_register_device(int device, virtio_setup_handler_t setup_handler);

#endif // __VIRTIO_VIRTIO_H__
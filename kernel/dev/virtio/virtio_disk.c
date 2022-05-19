//
// Created by shiroko on 22-5-18.
//

#include "./virtio.h"
#include <common/types.h>
#include <dev/dev.h>
#include <driver/console.h>
#include <lib/stdlib.h>
#include <lib/string.h>

void virtio_disk_setup(char *addr, int interrupt, int interrupt_parent) {
    kprintf("[DISK] Setup Virtio Disk at MMIO Bus 0x%lx.\n", addr);
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
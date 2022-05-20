//
// Created by shiroko on 22-5-18.
//

/*
 *  VFS -> Buffered IO Layer -> Actual Disk RW
 */

#include <dev/buffered_io.h>
#include <driver/console.h>

struct {
    spinlock_t  lock;
    list_head_t cache_head;
    int         cache_count;
} bio_cache;

// Device setup for buffered io
#include <dev/dev.h>

int init_buffered_io(dev_driver_t *drv) {
    kprintf("[BIO] Setup Buffered IO.\n");
    spinlock_init(&bio_cache.lock);
    bio_cache.cache_head = (list_head_t)LIST_HEAD_INIT(bio_cache.cache_head);
    return 0;
}

dev_driver_t bio_drv = {
    .name             = "buffered_io",
    .init             = init_buffered_io,
    .loading_sequence = 5, // load after all disk device setup.
    .dev_id           = 11 + 1,
    .major_ver        = 0,
    .minor_ver        = 1,
    .private_data     = NULL,
    .list             = LIST_HEAD_INIT(bio_drv.list),
};

ADD_DEV_DRIVER(bio_drv);

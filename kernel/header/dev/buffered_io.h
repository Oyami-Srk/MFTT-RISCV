//
// Created by shiroko on 22-5-20.
//

#ifndef __DEV_BUFFERED_IO_H__
#define __DEV_BUFFERED_IO_H__

// From xv6 buffered io

#include <common/types.h>
#include <lib/linklist.h>
#include <lib/sys/sleeplock.h>

#define MAX_BIO_CACHE 16
#define BUFFER_SIZE   512

typedef struct __buffered_io_t {
    bool     valid;
    uint16_t dev[2];
    uint64_t addr;
    char     data[BUFFER_SIZE];

    int         reference;
    sleeplock_t lock;
    list_head_t list;
} buffered_io_t;

buffered_io_t *bio_cache_get(uint16_t dev, uint64_t addr);

#endif // __DEV_BUFFERED_IO_H__
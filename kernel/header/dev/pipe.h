//
// Created by shiroko on 22-5-30.
//

#ifndef __DEV_PIPE_H__
#define __DEV_PIPE_H__

#include <lib/sys/spinlock.h>
#include <vfs.h>

#define PIPE_SIZE 512

typedef struct {
    spinlock_t lock;
    size_t     nread, nwrite;
    bool       read_open, write_open;
    char       data[PIPE_SIZE];
} pipe_t;

int pipe_create(file_t *reader, file_t *writer);

#endif // __DEV_PIPE_H__
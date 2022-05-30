//
// Created by shiroko on 22-5-30.
//

#include <dev/pipe.h>
#include <lib/string.h>
#include <memory.h>
#include <proc.h>
#include <stddef.h>
#include <vfs.h>

// xv6-impl

static int pipe_read(file_t *file, char *buffer, size_t offset, size_t len);
static int pipe_write(file_t *file, const char *buffer, size_t offset,
                      size_t len);
static int pipe_close(file_t *file);

static file_ops_t pipe_ops = {
    .open   = NULL,
    .close  = pipe_close,
    .read   = pipe_read,
    .write  = pipe_write,
    .flush  = NULL,
    .mmap   = NULL,
    .munmap = NULL,
    .seek   = NULL,
};

int pipe_create(file_t *reader, file_t *writer) {
    writer->f_inode = reader->f_inode = NULL;
    writer->f_dentry = reader->f_dentry = NULL;
    writer->f_offset = reader->f_offset = 0;
    writer->f_op = reader->f_op = &pipe_ops;
    writer->f_counts = reader->f_counts = 1;

    writer->f_mode = O_WRONLY;
    reader->f_mode = O_RDONLY;

    pipe_t *pipe = (pipe_t *)kmalloc(sizeof(pipe_t));
    if (!pipe)
        return -1;
    memset(pipe, 0, sizeof(pipe_t));

    spinlock_init(&pipe->lock);
    spinlock_acquire(&pipe->lock);
    pipe->read_open = pipe->write_open = true;
    pipe->nread = pipe->nwrite = 0;
    writer->f_fs_data = reader->f_fs_data = pipe;
    spinlock_release(&pipe->lock);

    return 0;
}

static int pipe_read(file_t *file, char *buffer, size_t offset, size_t len) {
    if (file->f_mode != O_RDONLY)
        return -1;
    pipe_t *pipe = file->f_fs_data;
    spinlock_acquire(&pipe->lock);
    while (pipe->nread == pipe->nwrite && pipe->write_open) {
        // write open, and no more write, wait
        sleep(&pipe->nread, &pipe->lock);
    }
    int i;
    for (i = 0; i < len; i++) {
        if (pipe->nread == pipe->nwrite) // no write
            break;
        // copy
        char c    = pipe->data[pipe->nread++ % PIPE_SIZE]; // circular buffer
        buffer[i] = c;
    }
    wakeup(&pipe->nwrite); // if write wait us
    spinlock_release(&pipe->lock);
    return i;
}

static int pipe_write(file_t *file, const char *buffer, size_t offset,
                      size_t len) {
    if (file->f_mode != O_WRONLY)
        return -1;
    pipe_t *pipe = file->f_fs_data;
    spinlock_acquire(&pipe->lock);
    int i;
    for (i = 0; i < len; i++) {
        while (pipe->nwrite == pipe->nread + PIPE_SIZE) {
            // write enough, wakeup for read
            if (pipe->read_open == false) {
                // on one could read, break
                spinlock_release(&pipe->lock);
                return -1;
            }
            wakeup(&pipe->nread);              // wakeup read
            sleep(&pipe->nwrite, &pipe->lock); // wait could write
        }
        // write
        pipe->data[pipe->nwrite++ % PIPE_SIZE] = buffer[i];
    }
    // complete, wakeup read
    wakeup(&pipe->nread);
    spinlock_release(&pipe->lock);
    return i;
}

static int pipe_close(file_t *file) {
    pipe_t *pipe = file->f_fs_data;
    spinlock_acquire(&pipe->lock);
    if (file->f_mode == O_WRONLY) {
        pipe->write_open = false;
        wakeup(&pipe->nread);
    } else {
        pipe->read_open = false;
        wakeup(&pipe->nwrite);
    }
    if (pipe->read_open == false && pipe->write_open == false) {
        // all close, turn off
        spinlock_release(&pipe->lock);
        kfree(pipe);
    } else {
        spinlock_release(&pipe->lock);
    }
}

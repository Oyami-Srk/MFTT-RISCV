//
// Created by shiroko on 22-5-16.
//
#include <dev.h>
#include <lib/sys/SBI.h>
#include <lib/sys/sleeplock.h>
#include <proc.h>
#include <vfs.h>

struct {
    sleeplock_t w_lock; // Write to console
    spinlock_t  r_lock; // Read from console
    // circular buffer
#define BUFFER_SIZE 256
    char   buffer[BUFFER_SIZE];
    char  *head; // point to next write
    char  *tail; // point to read
    size_t count;
} cons;

static inline char read_buffer() {
    spinlock_acquire(&cons.r_lock);
    char data;
    while (cons.count == 0) {
        // IMPORTANT: we assume that only proc context need to read buffer.
        sleep(&cons.count, &cons.r_lock);
    }
    data = *(cons.tail);
    cons.tail++;
    if (cons.tail == cons.buffer + BUFFER_SIZE)
        cons.tail = cons.buffer;
    cons.count--;
    spinlock_release(&cons.r_lock);
    return data;
}

static inline char peek_buffer(size_t offset) {
    spinlock_acquire(&cons.r_lock);
    char data;
    while (cons.count == 0) {
        // IMPORTANT: we assume that only proc context need to read buffer.
        sleep(&cons.count, &cons.r_lock);
    }
    if (cons.tail + offset >= cons.buffer + BUFFER_SIZE)
        data = *(cons.buffer +
                 (offset - (BUFFER_SIZE - (cons.buffer - cons.tail))));
    else
        data = *(cons.tail + offset);
    spinlock_release(&cons.r_lock);
    return data;
}

static inline void write_buffer(char data) {
    spinlock_acquire(&cons.r_lock);
    if (cons.count >= BUFFER_SIZE) {
        // 直接抛弃
        cons.tail++;
        if (cons.tail == cons.buffer + BUFFER_SIZE)
            cons.tail = cons.buffer;
        cons.count--;
    }
    *(cons.head) = data;
    cons.head++;
    if (cons.head == cons.buffer + BUFFER_SIZE)
        cons.head = cons.buffer;
    cons.count++;
    spinlock_release(&cons.r_lock);
    wakeup(&cons.count);
}

int console_write(struct vfs_file *file, const char *buffer, size_t offset,
                  size_t len) {
    sleeplock_acquire(&cons.w_lock);
    for (int i = 0; i < len; i++) {
        SBI_putchar(buffer[i]);
    }
    sleeplock_release(&cons.w_lock);
}

int console_read(struct vfs_file *file, char *buffer, size_t offset,
                 size_t len) {}

int init_console(dev_driver_t *drv) {
    cons.tail = cons.head = cons.buffer;
    cons.count            = 0;
    spinlock_init(&cons.r_lock);
    sleeplock_init(&cons.w_lock);
    return 0;
}

static struct vfs_inode_ops inode_ops = {
    .link = NULL, .lookup = NULL, .mkdir = NULL, .rmdir = NULL, .unlink = NULL};

static struct vfs_file_ops file_ops = {
    .flush  = NULL,
    .mmap   = NULL,
    .munmap = NULL,
    .write  = console_write,
    .read   = console_read,
    .open   = NULL,
    .close  = NULL,
    .seek   = NULL,
};

dev_driver_t console = {
    .name             = "console",
    .init             = init_console,
    .loading_sequence = 0,
    .dev_id           = 1,
    .major_ver        = 0,
    .minor_ver        = 1,
    .private_data     = NULL,
    .list             = LIST_HEAD_INIT(console.list),
};

ADD_DEV_DRIVER(console);
//
// Created by shiroko on 22-5-18.
//

/*
 *  VFS -> Buffered IO Layer -> Actual Disk RW
 */

#include <dev/buffered_io.h>
#include <dev/dev.h>
#include <driver/console.h>
#include <lib/string.h>

typedef int (*block_rw_t)(uint64_t lba, char *buf, size_t bytes, int func);

static struct {
    spinlock_t  lock;
    list_head_t cache_head;
    int         cache_count;
    block_rw_t  dev_rw[MAX_DEV_ID];
} bio_cache;

buffered_io_t *bio_cache_get(uint16_t dev, uint64_t addr) {
    spinlock_acquire(&bio_cache.lock);
    int i = 0;
    list_foreach_entry(&bio_cache.cache_head, buffered_io_t, list, buf) {
        i++;
        if (buf->dev[0] == dev && buf->addr == addr) {
            buf->reference++;
            // move to head
            list_del(&buf->list);
            list_add(&buf->list, &bio_cache.cache_head);

            spinlock_release(&bio_cache.lock);
            sleeplock_acquire(&buf->lock);
            return buf;
        }
    }
    // not in cache
    if (i >= MAX_BIO_CACHE) {
        // cached too many, we hope we could remove one
        kprintf("[BIO] Cached too many buffers, currently hold %d (max %d).\n",
                i + 1, MAX_BIO_CACHE);
        // but we do nothing
        // TODO: swap to disk and release.
    }
    buffered_io_t *buffer = (buffered_io_t *)kmalloc(sizeof(buffered_io_t));
    assert(buffer, "No memory while alloc buffered io.");
    memset(buffer, 0, sizeof(buffered_io_t));
    buffer->dev[0] = dev;
    buffer->addr   = addr;
    sleeplock_init(&buffer->lock);
    buffer->reference = 1;
    buffer->valid     = false;
    list_add(&buffer->list, &bio_cache.cache_head);
    spinlock_release(&bio_cache.lock);
    sleeplock_acquire(&buffer->lock);
    return buffer;
}

// return a loecked buffer
buffered_io_t *bio_cache_read(uint16_t dev, size_t addr) {
    assert(addr % BUFFER_SIZE == 0, "Addr must be aligned to buffer size.");
    buffered_io_t *buf = bio_cache_get(dev, addr);
    if (buf->valid)
        return buf;
    if (!bio_cache.dev_rw[dev])
        kpanic("No dev rw function to dev %d.", buf->dev[0]);
    bio_cache.dev_rw[dev](addr, buf->data, BUFFER_SIZE, 0);
    buf->valid = true;
    return buf;
}

// write to disk
void bio_cache_flush(buffered_io_t *buf) {
    if (!buf->lock.lock)
        kpanic("Not holding buffered io lock");
    if (!bio_cache.dev_rw[buf->dev[0]])
        kpanic("No dev rw function to dev %d.", buf->dev[0]);
    bio_cache.dev_rw[buf->dev[0]](buf->addr, buf->data, BUFFER_SIZE, 1);
}

// release buffer, no flush here
void bio_cache_release(buffered_io_t *buf) {
    if (!buf->lock.lock)
        kpanic("Not holding buffered io lock");
    sleeplock_release(&buf->lock);

    spinlock_acquire(&bio_cache.lock);
    buf->reference--;
    if (buf->reference == 0) {
        list_del(&buf->list);
        kfree((char *)buf);
    }
    spinlock_release(&bio_cache.lock);
}

// vfs pack for bio, fs not using this.
static int bio_read(file_t *file, char *buffer, size_t offset, size_t len) {
    int    dev                = file->f_inode->i_dev[1];
    size_t first_block_offset = offset % BUFFER_SIZE;
    size_t last_block_len     = len % BUFFER_SIZE;

    size_t start = ROUNDDOWN_WITH(BUFFER_SIZE, offset);
    size_t end = ROUNDDOWN_WITH(BUFFER_SIZE, start + first_block_offset + len);
    char  *pbuffer = buffer;
    for (size_t p = start; p <= end; p += BUFFER_SIZE) {
        size_t         c_offset = p == start ? first_block_offset : 0;
        size_t         c_len    = p == end ? last_block_len : BUFFER_SIZE;
        buffered_io_t *buf      = bio_cache_read(dev, p);
        memcpy(pbuffer, buf->data + c_offset, c_len);
        pbuffer += c_len;
        bio_cache_release(buf);
    }
}

static int bio_write(file_t *file, const char *buffer, size_t offset,
                     size_t len) {
    int    dev                = file->f_inode->i_dev[1];
    size_t first_block_offset = offset % BUFFER_SIZE;
    size_t last_block_len     = len % BUFFER_SIZE;

    size_t start = ROUNDDOWN_WITH(BUFFER_SIZE, offset);
    size_t end = ROUNDDOWN_WITH(BUFFER_SIZE, start + first_block_offset + len);
    char  *pbuffer = (char *)buffer;
    for (size_t p = start; p <= end; p += BUFFER_SIZE) {
        size_t         c_offset = p == start ? first_block_offset : 0;
        size_t         c_len    = p == end ? last_block_len : BUFFER_SIZE;
        buffered_io_t *buf      = NULL;
        if (c_offset || c_len != BUFFER_SIZE) {
            // read modity and write
            buf = bio_cache_read(dev, p);
        } else {
            buf = bio_cache_get(dev, p);
        }
        if (!buf)
            kpanic("Cannot get cached buffer.");
        memcpy(buf->data + c_offset, pbuffer, c_len);
        pbuffer += c_len;
        bio_cache_release(buf);
    }
}

static inode_ops_t inode_ops = {
    .link = NULL, .lookup = NULL, .mkdir = NULL, .rmdir = NULL, .unlink = NULL};

static file_ops_t file_ops = {
    .flush  = NULL,
    .mmap   = NULL,
    .munmap = NULL,
    .write  = bio_write,
    .read   = bio_read,
    .open   = NULL,
    .close  = NULL,
    .seek   = NULL,
};

static void setup_buffered_io_for(dentry_t *parent, dentry_t *dent) {
    kprintf("[BIO] Setup BIO For device %s.\n", dent->d_name);
    block_rw_t rw    = dent->d_inode->i_fs_data;
    inode_t   *inode = vfs_alloc_inode(NULL);
    inode->i_f_op    = &file_ops;
    inode->i_op      = &inode_ops;
    inode->i_fs_data = (void *)rw;
    inode->i_dev[0]  = DEV_BUFFERED_IO;
    uint16_t dev     = dent->d_inode->i_dev[1];
    inode->i_dev[1]  = dev;
    if (bio_cache.dev_rw[dev] && bio_cache.dev_rw[dev] != rw)
        kpanic("Dev id collsion.");
    bio_cache.dev_rw[dev] = rw;

    vfs_link_inode(inode, parent, dent->d_name + 4);
    kprintf("[BIO] Added Device File /dev/%s.\n", dent->d_name + 4);
}

// Device setup for buffered io

int init_buffered_io(dev_driver_t *drv) {
    kprintf("[BIO] Setup Buffered IO.\n");
    spinlock_init(&bio_cache.lock);
    bio_cache.cache_head = (list_head_t)LIST_HEAD_INIT(bio_cache.cache_head);

    // Read raw disk vfs inode
    dentry_t *devs = vfs_get_dentry("/dev", NULL);
    list_foreach_entry(&devs->d_subdirs, dentry_t, d_subdirs_list, dent) {
        if (memcmp("raw_", dent->d_name, 4) == 0) {
            setup_buffered_io_for(devs, dent);
        }
    }
    return 0;
}

dev_driver_t bio_drv = {
    .name             = "buffered_io",
    .init             = init_buffered_io,
    .loading_sequence = 5, // load after all disk device setup.
    .dev_id           = DEV_BUFFERED_IO,
    .major_ver        = 0,
    .minor_ver        = 1,
    .private_data     = NULL,
    .list             = LIST_HEAD_INIT(bio_drv.list),
};

ADD_DEV_DRIVER(bio_drv);

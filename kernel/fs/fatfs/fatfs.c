//
// Created by shiroko on 22-5-22.
//

#include "./fatfs.h"

extern superblock_t *do_mount_fatfs(superblock_t *sb, inode_t *dev,
                                    void *flags);

static filesystem_ops_t fatfs_fs_ops = {
    .init_fs = NULL,
    .mount   = do_mount_fatfs,
};
static filesystem_t fatfs = {
    .fs_name = "fat32", .fs_dev = 0, .fs_op = &fatfs_fs_ops};

ADD_FILESYSTEM(fatfs);
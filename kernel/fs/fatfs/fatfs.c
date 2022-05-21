//
// Created by shiroko on 22-5-22.
//

#include "./fatfs.h"

static filesystem_ops_t fatfs_fs_ops = {};
static filesystem_t     fatfs        = {
               .fs_name = "FatFs", .fs_dev = 0, .fs_op = &fatfs_fs_ops};

static int inode_idx = 1000;

ADD_FILESYSTEM(fatfs);
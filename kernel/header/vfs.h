//
// Created by shiroko on 22-5-2.
//

#ifndef __VFS_H__
#define __VFS_H__

/*
 * VFS的初始化不依赖任何硬盘IO，mount Flash/SD卡 的过程放在init进程里面。
 * VFS遍历定义在section里面的Filesystem定义。然后执行FS的初始化并mount
 * 一些设备文件。
 */

#include <common/types.h>
#include <lib/linklist.h>

// OPs

struct vfs_inode_ops {};

struct vfs_file_ops {};

struct vfs_superblock_ops {};

struct vfs_fs_ops {
    struct vfs_superblock *(*init_fs)(struct vfs_superblock *, void *);
};

// Defs
enum inode_type { inode_dev, inode_mount, inode_file, inode_dir };
#define VFS_DEV_BLOCK 1
#define VFS_DEV_CHAR  2
#define VFS_DEV_PIPE  3

struct vfs_inode {
    int      i_ino;
    int      i_nlinks;
    size_t   i_size;
    uint16_t i_dev[2];

    struct vfs_inode_ops  *i_op;
    struct vfs_superblock *i_sb;

    list_head_t i_sb_list;

    union {
    } info;
};

#define D_NAME_LEN 16
struct vfs_dir_entry {
    struct vfs_dir_entry *d_parent;
    // const char           *d_name;
    char              d_name[D_NAME_LEN];
    struct vfs_inode *d_inode;
};

struct vfs_superblock {
    uint16_t                   s_dev;
    struct vfs_superblock_ops *s_op;
    struct vfs_dir_entry      *s_root; // mount point
    list_head_t                inode_head;
};

struct vfs_file {
    struct vfs_inode     *f_inode;
    const char           *f_path;
    struct vfs_dir_entry *f_dentry;
};

struct filesystem_t {
    const char        *fs_name;
    uint16_t           fs_dev;
    struct vfs_fs_ops *fs_op;
};

void init_vfs();

#endif // __VFS_H__
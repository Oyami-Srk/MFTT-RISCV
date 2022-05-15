//
// Created by shiroko on 22-5-2.
//

#ifndef __VFS_H__
#define __VFS_H__

/*
 * VFS的初始化不依赖任何硬盘IO，mount Flash/SD卡 的过程放在init进程里面。
 * VFS遍历定义在section里面的Filesystem定义。然后执行FS的初始化并mount
 * 一些设备文件。
 * 和Linux不同，这里不需要rootfs，根是运行时固定的。同时，系统会占用/dev和
 * /sys作为固定的dentry。mount到根目录的文件系统中若存在/dev和/sys的话，
 * 可能会导致问题。（FIXME）
 */

#include <common/types.h>
#include <lib/linklist.h>
#include <lib/sys/spinlock.h>

struct vfs_inode;
struct vfs_dir_entry;
struct vfs_file;
struct vfs_superblock;
struct filesystem_t;

// OPs

struct vfs_inode_ops {
    // 在dentry目录项下创建一个新的inode，inode硬链接到dentry/name
    int (*create)(struct vfs_inode *inode, const char *name,
                  struct vfs_dir_entry *dentry);
    // 在目录项中寻找名字为name的目录项，并写入found_inode中
    int (*lookup)(struct vfs_inode *inode, const char *name,
                  struct vfs_dir_entry **found_inode);
    // 链接/取消链接 一个inode到dir里。
    int (*link)(struct vfs_inode *inode, struct vfs_inode *dir,
                const char *name);
    int (*unlink)(struct vfs_inode *dir, const char *name);
    // 创建/删除目录inode
    int (*mkdir)(struct vfs_inode *parent, const char *name);
    int (*rmdir)(struct vfs_inode *parent, const char *name);
};

struct vfs_file_ops {
    int (*seek)(struct vfs_file *file, size_t offset);
    int (*read)(struct vfs_file *file, char *buffer, size_t offset, size_t len);
    int (*write)(struct vfs_file *file, const char *buffer, size_t offset,
                 size_t len);
    // 内存映射，读写文件的时候其实是操作的内存中的缓冲区，mmap直接把内核缓冲映射到用户地址
    int (*mmap)(struct vfs_file *file, char *addr, size_t offset, size_t len);
    int (*munmap)(struct vfs_file *file, char *addr, size_t len);
    int (*flush)(struct vfs_file *file);
    int (*release)(struct vfs_file *file);
};

struct vfs_superblock_ops {};

struct vfs_fs_ops {
    struct vfs_superblock *(*init_fs)(struct vfs_superblock *, void *);
};

// Defs
enum inode_type { inode_dev, inode_mount, inode_file, inode_dir };
#define VFS_DEV_BLOCK 1
#define VFS_DEV_CHAR  2
#define VFS_DEV_PIPE  3

// Inode标识一个文件系统上唯一存在的文件对象。
struct vfs_inode {
    int      i_ino;
    int      i_nlinks; // hard links
    int      i_counts; // opened files counts, when decrease to 0, write.
    size_t   i_size;   // bytes size
    uint16_t i_dev[2]; // major, minor

    struct vfs_inode_ops  *i_op;
    struct vfs_superblock *i_sb;

    list_head_t i_sb_list; // 超级块中的inode链表

    spinlock_t spinlock;

    union {
    } info;

    void *i_fs_data;
};

// DirEntry标识一个目录inode内的每一项，包含文件和子目录。
#define D_NAME_LEN 16
struct vfs_dir_entry {
    struct vfs_dir_entry *d_parent;
    // const char           *d_name;
    char              d_name[D_NAME_LEN];
    struct vfs_inode *d_inode;
    list_head_t       d_subdirs_list; // DirEntry中的子目录项链表

    list_head_t d_subdirs; // -> d_subdirs_list;
};

struct vfs_superblock {
    uint16_t                   s_dev;
    struct vfs_superblock_ops *s_op;
    struct vfs_dir_entry      *s_root;     // mount point
    list_head_t                inode_head; // -> i_sb_list
};

// File标识一个被打开的文件实例。
struct vfs_file {
    struct vfs_inode     *f_inode;
    const char           *f_path;
    size_t                f_offset;
    struct vfs_dir_entry *f_dentry;
    struct vfs_file_ops  *f_op;
};

struct filesystem_t {
    const char        *fs_name;
    uint16_t           fs_dev;
    struct vfs_fs_ops *fs_op;
};

void init_vfs();

#endif // __VFS_H__
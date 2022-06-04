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

#include <lib/linklist.h>
#include <lib/sys/spinlock.h>
#include <types.h>

struct vfs_inode;
struct vfs_dir_entry;
struct vfs_file;
struct vfs_superblock;
struct vfs_filesystem_t;
struct vfs_read_dir_context;

typedef struct vfs_inode            inode_t;
typedef struct vfs_file             file_t;
typedef struct vfs_superblock       superblock_t;
typedef struct vfs_dir_entry        dentry_t;
typedef struct vfs_filesystem_t     filesystem_t;
typedef struct vfs_read_dir_context read_dir_context_t;
typedef struct vfs_mount            mount_t;

typedef int (*read_dir_callback)(dentry_t *dentry, void *callback_data);

// OPs

struct vfs_inode_ops {
    // 在目录项中寻找名字为name的目录项，并写入found_inode中
    int (*lookup)(inode_t *inode, const char *name, dentry_t **found_inode);
    // 链接/取消链接 一个inode到dir里。
    int (*link)(inode_t *inode, inode_t *dir, const char *name);
    int (*unlink)(inode_t *dir, const char *name);
    // 创建/删除目录inode
    int (*mkdir)(inode_t *parent, const char *name, inode_t **dir);
    int (*rmdir)(inode_t *parent, const char *name, inode_t **dir);
    // 读取目录，调用callback，纠结要不要在read_dir里直接插入，不用这个callback
    int (*read_dir)(inode_t *dir, read_dir_callback callback,
                    void *callback_data);
};

struct vfs_file_ops {
    int (*seek)(file_t *file, size_t offset);
    int (*read)(file_t *file, char *buffer, size_t offset, size_t len);
    int (*write)(file_t *file, const char *buffer, size_t offset, size_t len);
    // 内存映射，读写文件的时候其实是操作的内存中的缓冲区，mmap直接把内核缓冲映射到用户地址
    int (*mmap)(file_t *file, char *addr, size_t offset, size_t len);
    int (*munmap)(file_t *file, char *addr, size_t len);
    int (*flush)(file_t *file);
    int (*open)(file_t *file);
    int (*close)(file_t *file);
};

struct vfs_superblock_ops {
    // 创建一个新的inode
    inode_t *(*alloc_inode)(superblock_t *sb);
    int (*free_inode)(superblock_t *sb, inode_t *inode);
    int (*write_inode)(superblock_t *sb, inode_t *inode);
    int (*read_inode)(superblock_t *sb, inode_t *inode);
};

struct vfs_fs_ops {
    int (*init_fs)();
    superblock_t *(*mount)(superblock_t *, inode_t *, void *);
};

typedef struct vfs_inode_ops      inode_ops_t;
typedef struct vfs_file_ops       file_ops_t;
typedef struct vfs_superblock_ops superblock_ops_t;
typedef struct vfs_fs_ops         filesystem_ops_t;

// Defs
enum inode_type { inode_rootfs, inode_dev, inode_file, inode_dir };

#define VFS_DEV_BLOCK 1
#define VFS_DEV_CHAR  2
#define VFS_DEV_PIPE  3

// Inode标识一个文件系统上唯一存在的文件对象。
struct vfs_inode {
    int             i_ino;
    int             i_nlinks; // hard links
    int             i_counts; // opened files counts, when decrease to 0, write.
    size_t          i_size;   // bytes size
    uint16_t        i_dev[2]; // major for vfs and io, minor for dev driver.
    enum inode_type i_type;

    inode_ops_t  *i_op;
    superblock_t *i_sb;
    file_ops_t   *i_f_op;

    list_head_t i_sb_list; // 超级块中的inode链表

    spinlock_t spinlock;

    uint64_t i_atime, i_mtime, i_ctime;

    union {
    } info;

    void *i_fs_data;
};

struct vfs_mount {
    superblock_t *sb;
    inode_t      *root;
};

// DirEntry标识一个目录inode内的每一项，包含文件和子目录。
#define D_NAME_LEN        32
#define D_TYPE_FILE       1
#define D_TYPE_DIR        2
#define D_TYPE_NOT_LOADED 3
#define D_TYPE_MOUNTED    4

struct vfs_dir_entry {
    dentry_t *d_parent; // 目录项的父目录项
    // const char           *d_name;
    char        d_name[D_NAME_LEN]; // 目录项的名字
    inode_t    *d_inode;            // 目录项指向的inode
    list_head_t d_subdirs_list;     // DirEntry中的子目录项链表
    uint8_t     d_type;
    bool        d_loaded;
    mount_t    *d_mount;

    list_head_t d_subdirs; // -> d_subdirs_list;
};

struct vfs_superblock {
    uint16_t          s_dev[2];
    superblock_ops_t *s_op;

    inode_t    *s_root;       // root inode
    list_head_t s_inode_head; // -> i_sb_list, 一个超级块的所有inode链表

    void *s_fs_data;
};

// File标识一个被打开的文件实例。
struct vfs_file {
    // maybe a type here?
    struct vfs_inode     *f_inode;
    size_t                f_offset;
    struct vfs_dir_entry *f_dentry;
    struct vfs_file_ops  *f_op;
    int                   f_mode;
    int                   f_counts;

    void *f_fs_data;
};

struct vfs_read_dir_context {
    inode_t *d_inode;
    char     d_name[D_NAME_LEN];
    void    *d_fs_next;
};

struct vfs_filesystem_t {
    const char        *fs_name;
    uint16_t           fs_dev;
    struct vfs_fs_ops *fs_op;

    list_head_t fs_fslist;
};

// Funcs
void      init_vfs();
dentry_t *vfs_get_dentry(const char *path, dentry_t *cwd);
char     *vfs_get_dentry_fullpath(dentry_t *dent); // alloc by func
inode_t  *vfs_alloc_inode(superblock_t *sb);
int       vfs_write_inode(inode_t *inode);
int       vfs_link_inode(inode_t *inode, dentry_t *parent, const char *name);
superblock_t *vfs_create_superblock();
void          vfs_destroy_superblock(superblock_t *sb);

file_t *vfs_open(dentry_t *dentry, int mode);
file_t *vfs_fdup(file_t *old);
int     vfs_close(file_t *file);
int     vfs_read(file_t *file, char *buffer, size_t offset, size_t len);
int     vfs_write(file_t *file, const char *buffer, size_t offset, size_t len);
size_t  vfs_lseek(file_t *file, offset_t offset, int whence);

dentry_t *vfs_mkdir(dentry_t *parent, const char *path, int mode);
int       vfs_read_dir(file_t *parent, read_dir_context_t *context);

dentry_t *vfs_get_root();
int       vfs_mount(const char *dev, const char *mountpoint, const char *fstype,
                    void *flags);

// Utils for fs
#define ADD_FILESYSTEM(fs)                                                     \
    static filesystem_t *__ptr##fs                                             \
        __attribute__((used, section("Filesystems"))) = &fs

#endif // __VFS_H__
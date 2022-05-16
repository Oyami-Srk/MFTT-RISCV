//
// Created by shiroko on 22-5-14.
//

#include <driver/console.h>
#include <lib/string.h>
#include <memory.h>
#include <vfs.h>

struct vfs_dir_entry root_dentry = {
    .d_inode   = NULL,
    .d_name    = "/",
    .d_parent  = NULL,
    .d_subdirs = LIST_HEAD_INIT(root_dentry.d_subdirs),
};

struct vfs_dir_entry sys_dentry = {
    .d_inode   = NULL,
    .d_name    = "sys",
    .d_parent  = &root_dentry,
    .d_subdirs = LIST_HEAD_INIT(sys_dentry.d_subdirs),
};

struct vfs_dir_entry dev_dentry = {
    .d_inode   = NULL,
    .d_name    = "dev",
    .d_parent  = &root_dentry,
    .d_subdirs = LIST_HEAD_INIT(dev_dentry.d_subdirs),
};

void init_vfs() {
    kprintf("[VFS] Start initialize.\n");
    list_add(&sys_dentry.d_subdirs_list, &root_dentry.d_subdirs);
    list_add(&dev_dentry.d_subdirs_list, &root_dentry.d_subdirs);
}

struct vfs_dir_entry *vfs_get_dentry(const char           *path,
                                     struct vfs_dir_entry *cwd) {
    // TODO: better this method
    if (cwd == NULL || path[0] == '/')
        cwd = &root_dentry;

    char   fname[32];
    size_t len = strlen(path);

    if (path[len - 1] == '/')
        len--;

    int last = 0;
    for (int i = 0; i <= len; i++) {
        if (i == len || path[i] == '/') {
            memcpy(fname, path + last, i - last + 1);
            fname[i - last] = '\0';
            int  l          = i - last;
            bool found      = false;
            last            = i + 1;
            if (l == 0 || (l == 1 && fname[0] == '.')) {
                continue;
            } else if (l == 2 && fname[0] == '.' && fname[1] == '.') {
                if (cwd->d_parent)
                    cwd = cwd->d_parent;
                else
                    cwd = &root_dentry;
                continue;
            }
            kprintf("Lookup %s.\n", fname);
            list_foreach_entry(&cwd->d_subdirs, struct vfs_dir_entry,
                               d_subdirs_list, dir) {
                if (strcmp(fname, dir->d_name) == 0) {
                    cwd   = dir;
                    found = true;
                    break;
                }
            }
            if (!found) {
                struct vfs_inode *dir_inode = cwd->d_inode;
                if (!dir_inode || !dir_inode->i_op->lookup)
                    return NULL;
                struct vfs_dir_entry *r;
                int ret = dir_inode->i_op->lookup(dir_inode, fname, &r);
                if (ret != 0)
                    return NULL;
                list_add(&r->d_subdirs_list, &cwd->d_subdirs);
                cwd = r;
            }
        }
    }
    return cwd;
}

struct vfs_inode *vfs_alloc_inode(struct vfs_superblock *sb) {
    if (!sb) {
        // alloc by rootfs
        struct vfs_inode *inode =
            (struct vfs_inode *)kmalloc(sizeof(struct vfs_inode));
        memset(inode, 0, sizeof(struct vfs_inode));
        spinlock_init(&inode->spinlock);
        return inode;
    } else {
        if (!sb->s_op->alloc_inode)
            return NULL;
        return sb->s_op->alloc_inode(sb);
    }
}

int vfs_write_inode(struct vfs_inode *inode) {
    struct vfs_superblock *sb = inode->i_sb;
    if (sb) {
        if (!sb->s_op->write_inode)
            return -1;
        return sb->s_op->write_inode(sb, inode);
    } else {
        // write by rootfs
        return 0;
    }
}

int vfs_link_inode(struct vfs_inode *inode, struct vfs_dir_entry *parent,
                   const char *name) {
    // TODO: validate name
    int r = 0;
    if (!parent->d_inode || !inode->i_op->link) {
        // link by rootfs
    } else {
        r = inode->i_op->link(inode, parent->d_inode, name);
    }
    struct vfs_dir_entry *d =
        (struct vfs_dir_entry *)kmalloc(sizeof(struct vfs_dir_entry));
    memset(d, 0, sizeof(struct vfs_dir_entry));
    d->d_subdirs = (list_head_t)LIST_HEAD_INIT(d->d_subdirs);
    d->d_parent  = parent;
    if (inode->i_type == inode_dir)
        d->d_type = D_TYPE_DIR;
    else
        d->d_type = D_TYPE_FILE;
    strcpy(d->d_name, (char *)name);
    d->d_inode = inode;
    return r;
}

struct vfs_file *vfs_open(struct vfs_dir_entry *dentry, int mode) {
    struct vfs_file *file = (struct vfs_file *)kmalloc(sizeof(struct vfs_file));
    memset(file, 0, sizeof(struct vfs_file));
    file->f_dentry = dentry;
    file->f_inode  = dentry->d_inode;
    file->f_offset = 0;
    file->f_op     = file->f_inode->i_f_op;
    file->f_mode   = mode;
    if (file->f_op && file->f_op->open)
        if (file->f_op->open(file) != 0) {
            // err, TODO: handle this
            kpanic("err open.");
        }
    file->f_inode->i_counts++;
    return file;
}

int vfs_close(struct vfs_file *file) {}

int vfs_read(struct vfs_file *file, char *buffer, size_t offset, size_t len) {
    if (!file->f_op || file->f_op->read)
        return -1;
    return file->f_op->read(file, buffer, offset, len);
}

int vfs_write(struct vfs_file *file, const char *buffer, size_t offset,
              size_t len) {
    if (!file->f_op || file->f_op->write)
        return -1;
    return file->f_op->write(file, buffer, offset, len);
}

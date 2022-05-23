//
// Created by shiroko on 22-5-14.
//

#include <driver/console.h>
#include <lib/string.h>
#include <memory.h>
#include <proc.h>
#include <vfs.h>

inode_t root_inode = {.i_ino = 1, .i_size = 0};

dentry_t root_dentry = {
    .d_inode   = &root_inode,
    .d_name    = "/",
    .d_parent  = NULL,
    .d_subdirs = LIST_HEAD_INIT(root_dentry.d_subdirs),
};

dentry_t sys_dentry = {
    .d_inode   = NULL,
    .d_name    = "sys",
    .d_parent  = &root_dentry,
    .d_subdirs = LIST_HEAD_INIT(sys_dentry.d_subdirs),
};

dentry_t dev_dentry = {
    .d_inode   = NULL,
    .d_name    = "dev",
    .d_parent  = &root_dentry,
    .d_subdirs = LIST_HEAD_INIT(dev_dentry.d_subdirs),
};

LIST_HEAD(filesystems_list);

void init_vfs() {
    kprintf("[VFS] Start initialize.\n");
    list_add(&sys_dentry.d_subdirs_list, &root_dentry.d_subdirs);
    list_add(&dev_dentry.d_subdirs_list, &root_dentry.d_subdirs);
    // build fs list
    kprintf("[VFS] Register filesystem: ");
    section_foreach_entry(Filesystems, filesystem_t *, fs) {
        kprintf("%s", (*fs)->fs_name);
        int ret;
        if ((*fs)->fs_op->init_fs && (ret = (*fs)->fs_op->init_fs()) != 0) {
            kprintf("(failed)");
            continue;
        }
        if (!section_is_last(Filesystems, filesystem_t *, fs))
            kprintf(",");
        list_add(&(*fs)->fs_fslist, &filesystems_list);
    }
    kprintf("\n");
}

dentry_t *vfs_get_parent_dentry(char const *path, dentry_t *cwd, char *name) {
    // TODO: consider cache the search
    if (cwd == NULL)
        cwd = &root_dentry;
    if (*path == '/')
        cwd = &root_dentry, path++;
    if (!*path) {
        name[0] = '/';
        name[1] = '\0';
        return NULL;
    }
    dentry_t   *par = cwd;
    char        fname[32];
    char        dname[32];
    char const *last = path;
    bool        meet = false;
    for (; *path; path++) {
        if (!*path || *path == '/') {
            memcpy(dname, last, path - last);
            dname[path - last] = '\0';
            meet               = true;
            last               = path + 1;
        } else if (meet) {
            if (strcmp("..", dname) == 0) {
                if (par->d_parent)
                    par = par->d_parent;
                else
                    par = &root_dentry;
            } else if (dname[0] == '\0' || strcmp(".", dname) == 0) {
                // skip
            } else {
                bool found = false;
                list_foreach_entry(&par->d_subdirs, dentry_t, d_subdirs_list,
                                   dir) {
                    if (strcmp(dname, dir->d_name) == 0) {
                        par   = dir;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    inode_t *dir_inode = par->d_inode;
                    if (!dir_inode || !dir_inode->i_op->lookup)
                        return NULL;
                    dentry_t *r;
                    int ret = dir_inode->i_op->lookup(dir_inode, dname, &r);
                    if (ret != 0 || !r)
                        return NULL;
                    list_add(&r->d_subdirs_list, &cwd->d_subdirs);
                    par = r;
                }
            }
            meet = false;
        }
    }
    strcpy(name, (char *)last);
    return par;
}

dentry_t *vfs_get_dentry(const char *path, dentry_t *cwd) {
    char      cname[32];
    dentry_t *parent = vfs_get_parent_dentry(path, cwd, cname);
    if (!parent) {
        if (strcmp("/", cname) == 0)
            return &root_dentry;
        return NULL;
    }
    if (strcmp("..", cname) == 0)
        return parent->d_parent;
    else if (strcmp(".", cname) == 0)
        return parent;
    list_foreach_entry(&parent->d_subdirs, dentry_t, d_subdirs_list, dir) {
        if (strcmp(cname, dir->d_name) == 0) {
            return dir;
        }
    }
    // not found
    inode_t *dir_inode = parent->d_inode;
    if (!dir_inode || !dir_inode->i_op->lookup)
        return NULL;
    dentry_t *r;
    int       ret = dir_inode->i_op->lookup(dir_inode, cname, &r);
    if (ret != 0 || !r)
        return NULL;
    list_add(&r->d_subdirs_list, &cwd->d_subdirs);
    return r;
}

inode_t *vfs_alloc_inode(superblock_t *sb) {
    if (!sb) {
        // alloc by rootfs
        inode_t *inode = (inode_t *)kmalloc(sizeof(inode_t));
        memset(inode, 0, sizeof(inode_t));
        spinlock_init(&inode->spinlock);
        return inode;
    } else {
        if (!sb->s_op->alloc_inode)
            return NULL;
        return sb->s_op->alloc_inode(sb);
    }
}

int vfs_write_inode(inode_t *inode) {
    superblock_t *sb = inode->i_sb;
    if (sb) {
        if (!sb->s_op->write_inode)
            return -1;
        return sb->s_op->write_inode(sb, inode);
    } else {
        // write by rootfs
        return 0;
    }
}

int vfs_link_inode(inode_t *inode, dentry_t *parent, const char *name) {
    // TODO: validate name
    int r = 0;
    if (!parent->d_inode || !inode->i_op->link) {
        // link by rootfs
        inode->i_nlinks++;
    } else {
        r = inode->i_op->link(inode, parent->d_inode, name);
    }
    dentry_t *d = (dentry_t *)kmalloc(sizeof(dentry_t));
    memset(d, 0, sizeof(dentry_t));
    d->d_subdirs = (list_head_t)LIST_HEAD_INIT(d->d_subdirs);
    d->d_parent  = parent;
    if (inode->i_type == inode_dir)
        d->d_type = D_TYPE_DIR;
    else
        d->d_type = D_TYPE_FILE;
    strcpy(d->d_name, (char *)name);
    d->d_inode = inode;
    list_add(&d->d_subdirs_list, &parent->d_subdirs);
    return r;
}

file_t *vfs_open(dentry_t *dentry, int mode) {
    file_t *file = (file_t *)kmalloc(sizeof(file_t));
    memset(file, 0, sizeof(file_t));
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

int vfs_close(file_t *file) {}

int vfs_read(file_t *file, char *buffer, size_t offset, size_t len) {
    if (!file->f_op || !file->f_op->read)
        return -1;
    int r = file->f_op->read(file, buffer, file->f_offset + offset, len);
    file->f_offset += len;
    return r;
}

int vfs_write(file_t *file, const char *buffer, size_t offset, size_t len) {
    if (!file->f_op || !file->f_op->write)
        return -1;
    int r = file->f_op->write(file, buffer, file->f_offset + offset, len);
    file->f_offset += len;
    return r;
}

dentry_t *vfs_mkdir(dentry_t *parent, const char *path, int mode) {
    char dname[32];
    parent = vfs_get_parent_dentry(path, parent, dname);
    if (dname[0] == '.' &&
        (dname[1] == '\0' || (dname[2] == '.' && dname[3] == '\0')))
        return NULL; // try to mkdir parent or self
    inode_t *pinode = parent->d_inode;
    inode_t *dinode = NULL;
    list_foreach_entry(&parent->d_subdirs, dentry_t, d_subdirs_list, dent) {
        if (strcmp(dname, dent->d_name) == 0)
            return NULL; // existed
    }
    if (pinode->i_op && pinode->i_op->mkdir) {
        if (pinode->i_op->mkdir(pinode, dname, &dinode) != 0)
            return NULL; // failed
    } else {
        dinode = vfs_alloc_inode(pinode->i_sb);
    }
    if (!dinode)
        return NULL; // no dinode created.
    // add to dentries
    dentry_t *new = (dentry_t *)kmalloc(sizeof(dentry_t));
    memset(new, 0, sizeof(dentry_t));
    new->d_subdirs = (list_head_t)LIST_HEAD_INIT(new->d_subdirs);
    new->d_inode   = dinode;
    new->d_parent  = parent;
    new->d_type    = D_TYPE_DIR;
    strcpy(new->d_name, dname);
    list_add(&new->d_subdirs_list, &parent->d_subdirs);
    return new;
}

dentry_t *vfs_get_root() { return &root_dentry; }

int vfs_mount(const char *dev, const char *mountpoint, const char *fstype,
              void *flags) {
    // search fs
    filesystem_t *fs = NULL;
    list_foreach_entry(&filesystems_list, filesystem_t, fs_fslist, fs_) {
        if (strcmp(fstype, fs_->fs_name) == 0)
            fs = fs_;
    }
    if (!fs)
        return -1;
    dentry_t *cwd = NULL;
    if (myproc())
        cwd = myproc()->cwd;
    dentry_t *dent_dev = vfs_get_dentry(dev, cwd);
    dentry_t *dent_mp  = vfs_get_dentry(mountpoint, cwd);
    if (!dent_dev)
        return -3;
    if (!dent_mp)
        return -4;
    if (dent_mp->d_type == D_TYPE_MOUNTED)
        return -6; // already mounted
    // TODO: check fs dev type
    if (!fs->fs_op->mount)
        return -2;
    superblock_t *sb     = vfs_create_superblock();
    *sb                  = (superblock_t){.s_op      = NULL,
                                          .s_dev[0]  = dent_dev->d_inode->i_dev[0],
                                          .s_dev[1]  = dent_dev->d_inode->i_dev[1],
                                          .s_fs_data = NULL,
                                          .s_root    = NULL};
    superblock_t *ret_sb = fs->fs_op->mount(sb, dent_dev->d_inode, flags);
    if (!ret_sb) {
        vfs_destroy_superblock(sb);
        return -5;
    }
    // mount superblock
    dent_mp->d_inode = ret_sb->s_root;
    dent_mp->d_type  = D_TYPE_MOUNTED;
    return 0;
}

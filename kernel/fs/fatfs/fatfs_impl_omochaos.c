//
// Created by shiroko on 22-5-22.
//
#include "./fatfs.h"
#include <dev/buffered_io.h>
#include <lib/string.h>
#include <types.h>

#define __FAT_FS_DEBUG__ 0
#include <driver/console.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static size_t HD_drv_read(uint16_t drv, uint32_t lba, char *buf, size_t bytes) {
    bio_cache_read(drv, lba);
    return bytes;
}

/* 我之前在OmochaOS上使用的十分简单的FAT32文件系统实现，之后可能会使用FatFs作为实现
 * 只有读取没有写入
 */
struct fs_file_info {
    char     filename[8 + 3 + 1];
    size_t   size; // in byte
    uint32_t clus;
};

struct FAT32_BS {
    uint8_t  jmpBoot[3];
    uint8_t  OEMName[8];
    uint16_t BytesPerSec;
    uint8_t  SecPerClus;
    uint16_t RsvdSecCnt;
    uint8_t  NumFATs;
    uint16_t RootEntCnt; // 0 in fat 32
    uint16_t TotSec16;
    uint8_t  Media;
    uint16_t FATSz16; // 0 in fat 32
    uint16_t SecPerTrk;
    uint16_t NumHeads;
    uint32_t HiddSec;
    uint32_t TotSec32;
    //=======
    uint32_t FATSz32;
    uint16_t ExtFlags;
    uint16_t FSVer;
    uint32_t RootClus;
    uint16_t FSInfo;
    uint16_t BkBootSec;
    uint8_t  Reserved[12];
    uint8_t  DrvNum;
    uint8_t  Reserved1;
    uint8_t  BootSig; // 0x29
    uint32_t VolID;
    char     VolLab[11];
    uint8_t  FileSysType[8];
} __attribute__((packed));

union FAT32_DirEnt {
    struct {
        char     Name[8];
        char     Ext[3];
        uint8_t  Attr;
        uint8_t  NTRes;
        uint8_t  CrtTimeTenth;
        uint16_t CrtTime;
        uint16_t CrtDate;
        uint16_t LastAccDate;
        uint16_t FstClusHI;
        uint16_t WrtTime;
        uint16_t WrtDate;
        uint16_t FstClusLO;
        uint32_t FileSize;
    } __attribute__((packed));

    struct {
        uint8_t  L_Ord;
        char     L_Name1[10]; // 2B a char, totally 5 chars;
        uint8_t  L_Attr;
        uint8_t  L_Type;
        uint8_t  L_ChkSum;
        char     L_Name2[12]; // 2B a char, totally 6 chars;
        uint16_t L_FstClusLO; // be zero
        char     L_Name3[4];  // 2B a char, totally 2 chars;
    } __attribute__((packed));
}; // 32 bytes

struct FAT32_FSInfo {
    uint32_t StrucSig;
    uint32_t FreeCount;
    uint32_t NxtFree;
    uint8_t  Rsvd[12];
    uint32_t TrailSig;
} __attribute__((packed));

struct FAT32_FileSystem {
    uint8_t  OEMName[9];
    size_t   BytesPerSec;
    uint32_t SecPerClus;
    uint32_t FATstartSct;
    uint8_t  NumFATs;
    uint32_t HiddenSector;
    uint32_t TotalSector;
    uint32_t FATSize; // in sector
    uint16_t ExtFlags;
    uint32_t RootClus;
    uint16_t FSInfo;
    uint16_t BackupBootSector;
    uint8_t  Signature;
    uint32_t VolumeID;
    char     VolumeLabel[12];
    char     FileSystemType[9];
    uint32_t FirstDataClus;
    uint16_t drv;
    uint32_t FreeClusCount;
    uint32_t NextFreeClusCount;
};

#define ATTR_READ_ONLY 0x1
#define ATTR_HIDDEN    0x2
#define ATTR_SYSTEM    0x4
#define ATTR_VOLUME_ID 0x8
#define ATTR_DIR       0x10
#define ATTR_ARCHIVE   0x20
#define FS_ATTR_LONGNAME                                                       \
    (ATTR_READ_ONLY | ATTR_VOLUME_ID | ATTR_SYSTEM | ATTR_HIDDEN)

#define CLUS2SECTOR(fs, N)                                                     \
    (((fs)->FirstDataClus) + ((N - (fs)->RootClus) * (fs)->SecPerClus))

static uint8_t checksum_fname(char *fname) {
    uint8_t i;
    uint8_t checksum = 0;
    for (i = 0; i < 11; i++) {
        uint8_t highbit = (checksum & 0x1) << 7;
        checksum        = ((checksum >> 1) & 0x7F) | highbit;
        checksum        = checksum + fname[i];
    }
    return checksum;
}

static int fat_read_superblock(uint16_t drv, struct FAT32_FileSystem *fs) {
    buffered_io_t  *buf  = bio_cache_read(drv, 0);
    char           *pBuf = buf->data;
    struct FAT32_BS BootSector;
    memset(&BootSector, 0, sizeof(struct FAT32_BS));
    memcpy(&BootSector, pBuf, sizeof(struct FAT32_BS));
    pBuf = NULL;
    bio_cache_release(buf);
    char VolLab[12];
    char OEMName[9];
    char FileSysType[9];
    memset(VolLab, 0, sizeof(VolLab));
    memset(OEMName, 0, sizeof(OEMName));
    memset(FileSysType, 0, sizeof(FileSysType));
    memcpy(VolLab, BootSector.VolLab, 11);
    memcpy(OEMName, BootSector.OEMName, 8);
    memcpy(FileSysType, BootSector.FileSysType, 8);
#if __FAT_FS_DEBUG__
    kprintf("[FATFS] jmpBoot: 0x%x 0x%x 0x%x, OEMName: %s, BytesPerSec: %d, "
            "SecPerClus: %d, "
            "RsvdSecCnt: %d, NumFATs: %d, RootEntCnt: %d, TotSec16: %d, Media: "
            "%d, FATSz16: %d, SecPerTrk: %d, NumHeads: %d, HiddSec: %d\n",
            BootSector.jmpBoot[0], BootSector.jmpBoot[1], BootSector.jmpBoot[2],
            OEMName, BootSector.BytesPerSec, BootSector.SecPerClus,
            BootSector.RsvdSecCnt, BootSector.NumFATs, BootSector.RootEntCnt,
            BootSector.TotSec16, BootSector.Media, BootSector.FATSz16,
            BootSector.SecPerTrk, BootSector.NumHeads, BootSector.HiddSec);
    kprintf("[FATFS] TotSec32: %d, FATSz32: %d, ExtFlags: 0x%x, FSVer: 0x%x, "
            "RootClus: "
            "%d, FSInfo: %d, BkBootSec: %d, DrvNum: %d, BootSig: 0x%x, VolID: "
            "0x%x, "
            "VolLab: %s\n",
            BootSector.TotSec32, BootSector.FATSz32, BootSector.ExtFlags,
            BootSector.FSVer, BootSector.RootClus, BootSector.FSInfo,
            BootSector.BkBootSec, BootSector.DrvNum, BootSector.BootSig,
            BootSector.VolID, VolLab);
#endif
    assert(BootSector.BootSig == 0x29, "FAT BootSig invaild.");
    assert(BootSector.TotSec16 == 0, "FAT TotSec16 invalid");
    assert(BootSector.FATSz16 == 0, "FAT FATsz16 failed");
    assert(BootSector.RootEntCnt == 0, "FAT RootEntCnt failed");
    assert(BootSector.BytesPerSec == 512, "FAT BytesPerSec failed");
    memset(fs, 0, sizeof(struct FAT32_FileSystem));
    memcpy(fs->OEMName, OEMName, sizeof(OEMName));
    memcpy(fs->VolumeLabel, VolLab, sizeof(VolLab));
    memcpy(fs->FileSystemType, FileSysType, sizeof(FileSysType));
    fs->BytesPerSec      = BootSector.BytesPerSec;
    fs->SecPerClus       = BootSector.SecPerClus;
    fs->NumFATs          = BootSector.NumFATs;
    fs->HiddenSector     = BootSector.HiddSec;
    fs->TotalSector      = BootSector.TotSec32;
    fs->FATSize          = BootSector.FATSz32;
    fs->ExtFlags         = BootSector.ExtFlags;
    fs->RootClus         = BootSector.RootClus;
    fs->FSInfo           = BootSector.FSInfo;
    fs->BackupBootSector = BootSector.BkBootSec;
    fs->Signature        = BootSector.BootSig;
    fs->VolumeID         = BootSector.VolID;
    fs->drv              = drv;
    fs->FirstDataClus =
        BootSector.RsvdSecCnt + BootSector.FATSz32 * BootSector.NumFATs;
    fs->FATstartSct = BootSector.RsvdSecCnt;

    uint64_t fsinfo_lba = fs->FSInfo;
    buf                 = bio_cache_read(drv, fsinfo_lba * fs->BytesPerSec);
    pBuf                = buf->data;

    struct FAT32_FSInfo FSInfo;
    assert(*((uint32_t *)pBuf) == 0x41615252, "FAT LeadSig invalid");
    memcpy(&FSInfo, pBuf + 484, sizeof(struct FAT32_FSInfo));
    assert(FSInfo.StrucSig == 0x61417272, "FAT StructSig invalid");
    assert(FSInfo.TrailSig == 0xAA550000, "FAT Trail invalid");
    fs->FreeClusCount     = FSInfo.FreeCount;
    fs->NextFreeClusCount = FSInfo.NxtFree;
    bio_cache_release(buf);
}

static inline char char_upper(char c) {
    return c - (c >= 'a' && c <= 'z' ? 32 : 0);
}

// read_8_3_filename assume that thr length of buffer is >= 12
// turn 8.3fn into normal one
static void read_8_3_filename(char *fname, char *buffer) {
    memcpy(buffer, fname, 8);
    char *p = buffer + 7;
    while (p >= buffer && *p == ' ')
        p--;
    p++;
    if (*(fname + 8) == ' ') {
        *p = '\0';
        return;
    }
    *p = '.';
    memcpy(++p, fname + 8, 3);
    *(p + 3) = '\0';
    p += 2;
    while (*p == ' ')
        *(p--) = '\0';
}

// turn normal fn into 8.3 one
static void write_8_3_filename(char *fname, char *buffer) {
    memset(buffer, ' ', 11);
    uint32_t namelen = strlen((const char *)fname);
    // find the extension
    int i;
    int dot_index = -1;
    for (i = namelen - 1; i >= 0; i--) {
        if (fname[i] == '.') {
            // Found it!
            dot_index = i;
            break;
        }
    }

    // Write the extension
    if (dot_index >= 0) {
        for (i = 0; i < 3; i++) {
            uint32_t c_index = dot_index + 1 + i;
            uint8_t  c = c_index >= namelen ? ' ' : char_upper(fname[c_index]);
            buffer[8 + i] = c;
        }
    } else {
        for (i = 0; i < 3; i++) {
            buffer[8 + i] = ' ';
        }
    }

    // Write the filename.
    uint32_t firstpart_len = namelen;
    if (dot_index >= 0) {
        firstpart_len = dot_index;
    }
    if (firstpart_len > 8) {
        // Write the weird tilde thing.
        for (i = 0; i < 6; i++) {
            buffer[i] = char_upper(fname[i]);
        }
        buffer[6] = '~';
        buffer[7] = '1'; // probably need to enumerate like files and increment.
    } else {
        // Just write the file name.
        uint32_t j;
        for (j = 0; j < firstpart_len; j++) {
            buffer[j] = char_upper(fname[j]);
        }
    }
}

static inline uint32_t get_next_clus_in_FAT(struct FAT32_FileSystem *fs,
                                            uint32_t                 clus) {
    // 32 bit a fat ent(4 byte)
    uint32_t sector_of_clus_in_fat = clus / 128;

    buffered_io_t *buf = bio_cache_read(
        fs->drv, (fs->FATstartSct + sector_of_clus_in_fat) * fs->BytesPerSec);
    char    *pBuf      = buf->data;
    uint32_t next_clus = ((uint32_t *)pBuf)[clus - 128 * sector_of_clus_in_fat];
    bio_cache_release(buf);
    return next_clus & 0x0FFFFFFF;
}

void read_a_clus(struct FAT32_FileSystem *fs, uint32_t clus, void *buf,
                 size_t size) {
    if (size < fs->BytesPerSec * fs->SecPerClus) {
        kpanic("buf size smaller than BytesPerClus");
    }
    HD_drv_read(fs->drv, CLUS2SECTOR(fs, clus), buf,
                fs->BytesPerSec * fs->SecPerClus);
}

// return actually read size
size_t read_file(struct FAT32_FileSystem *fs, struct fs_file_info *fileinfo,
                 uint32_t offset, uint8_t *buf, size_t size) {
    if (offset + size > fileinfo->size)
        size = fileinfo->size - offset;
    uint32_t BytesPerClus = 512 * fs->SecPerClus;

    uint32_t clus = fileinfo->clus;
    for (uint32_t p = BytesPerClus; p <= offset; p += BytesPerClus) {
        clus = get_next_clus_in_FAT(fs, clus);
    }
    uint32_t p_clus = offset % BytesPerClus;
    size_t   r_size = size;
    if (p_clus % 512) {
        size_t s = MIN(512 - (p_clus % 512), r_size);
        //        HD_drv_read(fs->drv, CLUS2SECTOR(fs, clus), hd_buf_1, 512);
        //        memcpy(buf, hd_buf_1 + (p_clus % 512), s);
        buf += s; // buf is sector aligned now
        p_clus += s;
        r_size -= s;
    }
    if (r_size == 0)
        return size;
    if (p_clus == BytesPerClus) {
        clus   = get_next_clus_in_FAT(fs, clus);
        p_clus = 0;
    } else if (p_clus > BytesPerClus) {
        kpanic("Error of p_clus' value!");
    }
    for (; r_size > 512;) {
        HD_drv_read(fs->drv, CLUS2SECTOR(fs, clus) + p_clus / 512, buf, 512);
        r_size -= 512;
        p_clus += 512;
        buf += 512;
        if (p_clus >= BytesPerClus) {
            clus   = get_next_clus_in_FAT(fs, clus);
            p_clus = 0;
        }
    }
    if (r_size) {
        HD_drv_read(fs->drv, CLUS2SECTOR(fs, clus) + p_clus / 512, buf, r_size);
    }
    return size;

    /*
    uint32_t start_sector_in_first_clus   = (offset % BytesPerClus) / 512;
    uint32_t start_offset_in_first_sector = offset % 512;

    uint32_t full_clus_count =
        (size - (fs->SecPerClus - start_sector_in_first_clus) * 512) /
        BytesPerClus;
    */

    return size;
}

// =-=-=-=-=-=-=-=-=-=-=-= VFS Impl =-=-=-=-=-=-=-=-=-=-=-=-=-=

#include <lib/rb_tree.h>

static int inode_idx = 1000;

static rb_tree inode_tree = {.root = NULL};
// TODO: build hashmap for path to inode

static inode_t *alloc_inode(superblock_t *sb);

typedef struct {
    uint32_t start_clus;
} fatfs_inode_data_t;

typedef struct {
    inode_t *inode;
    rb_node  rb_node;
} fatfs_inode_index_t;

static int open(file_t *file) { return 0; }
static int read(file_t *file, char *buffer, size_t offset, size_t len) {
    fatfs_inode_data_t *fidata = (fatfs_inode_data_t *)file->f_inode->i_fs_data;
    uint32_t            clus   = fidata->start_clus;
    struct FAT32_FileSystem *fs = file->f_inode->i_sb->s_fs_data;

    if (offset + len > file->f_inode->i_size)
        len = file->f_inode->i_size - offset;
    uint32_t BytesPerClus = 512 * fs->SecPerClus;

    for (uint32_t p = BytesPerClus; p <= offset; p += BytesPerClus) {
        clus = get_next_clus_in_FAT(fs, clus); // walk to offset's clus
    }

    uint32_t p_clus = offset % BytesPerClus;
    size_t   r_size = len;

    if (p_clus % 512) {
        size_t         s = MIN(512 - (p_clus % 512), r_size);
        buffered_io_t *buf =
            bio_cache_read(fs->drv, CLUS2SECTOR(fs, clus) * fs->BytesPerSec);
        memcpy(buffer, buf->data + (p_clus % 512), s);
        buffer += s; // buf is sector aligned now
        p_clus += s;
        r_size -= s;
        bio_cache_release(buf);
    }
    if (r_size == 0)
        return len;
    if (p_clus == BytesPerClus) {
        clus   = get_next_clus_in_FAT(fs, clus);
        p_clus = 0;
    } else if (p_clus > BytesPerClus) {
        kpanic("Error of p_clus' value!");
    }
    for (; r_size > 512;) {
        buffered_io_t *buf = bio_cache_read(
            fs->drv, (CLUS2SECTOR(fs, clus) + p_clus / 512) * fs->BytesPerSec);
        memcpy(buffer, buf->data, BUFFER_SIZE);
        bio_cache_release(buf);
        r_size -= 512;
        p_clus += 512;
        buffer += 512;
        if (p_clus >= BytesPerClus) {
            clus   = get_next_clus_in_FAT(fs, clus);
            p_clus = 0;
        }
    }
    if (r_size) {
        buffered_io_t *buf = bio_cache_read(
            fs->drv, (CLUS2SECTOR(fs, clus) + p_clus / 512) * fs->BytesPerSec);
        memcpy(buffer, buf->data, r_size);
        bio_cache_release(buf);
    }
    return len;
}

static int write(file_t *file, const char *buffer, size_t offset, size_t len) {
    return 0;
}
static int close(file_t *file) { return 0; }
static int flush(file_t *file) { return 0; }
static int seek(file_t *file, size_t offset) { return 0; }
static int mmap(file_t *file, char *addr, size_t offset, size_t len) {
    /* FIXME: vfs impl should not relay on other data such as proc, intr.
     * Got memory outside mmap and leave vfs mother do this
     */
    proc_t *proc = myproc();
    assert((uintptr_t)addr % PG_SIZE == 0,
           "mmap addr must be aligned to page.");
    struct FAT32_FileSystem *fs = file->f_inode->i_sb->s_fs_data;
    // TODO: impl mmap require bufferio's buffer set to PGSIZE
    return 0;
}
static int munmap(file_t *file, char *addr, size_t len) { return 0; }

static file_ops_t file_ops = {
    .write = write,
    .read  = read,
    // .open   = open,
    .seek   = seek,
    .munmap = munmap,
    .mmap   = mmap,
    .flush  = flush,
    .close  = close,
};

typedef int (*fat_dirent_loop_callback_t)(union FAT32_DirEnt *dirent,
                                          char *long_name, void *data);
static void loop_fat_dirent(struct FAT32_FileSystem *fs, uint32_t dir_clus,
                            fat_dirent_loop_callback_t callback, void *data) {
    /*
     * FAT32在短文件项之前总会倒序填充长文件名项，若long_name为NULL的情况下遇见
     * 长文件名项，则是最后一个长文件名项。这里用的是Unicode存储，我们不支持Unicode
     * 所以只截取最低的字节为ASCII。一个长文件名项可以存储13个字符。
     */
    char   *long_name        = NULL; // dynamic alloc while reading
    char   *pLong            = NULL;
    uint8_t long_name_chksum = 0;

    for (;;) {
        for (uint32_t i = 0; i < fs->SecPerClus; i++) {
            buffered_io_t *buf = bio_cache_read(
                fs->drv, (CLUS2SECTOR(fs, dir_clus) + i) * fs->BytesPerSec);
            char *pBuf = buf->data;

            union FAT32_DirEnt DirEnt;
            for (uint32_t offset = 0; offset < 512;
                 offset += sizeof(union FAT32_DirEnt)) {
                memcpy(&DirEnt, pBuf + offset, sizeof(union FAT32_DirEnt));
                if (DirEnt.Name[0] == 0)
                    break;
                if (DirEnt.Name[0] == 0xE5 || DirEnt.Name[0] == 0x05) {
                    continue;
                }
                if (DirEnt.Name[0] == 0x2E) {
                    continue;
                }
                if (DirEnt.Attr == FS_ATTR_LONGNAME) {
                    // must got long name...
                    int ord = DirEnt.L_Ord & 0x1F;
                    if (long_name == NULL) {
                        // first meet
                        assert(DirEnt.L_Ord & 0x40, "Order mistake.");
                        size_t sz = sizeof(char) * ord * 13 + 1;
                        long_name = kmalloc(sz);
                        assert(long_name, "OOM");
                        pLong            = long_name + sz - 1;
                        long_name_chksum = DirEnt.L_ChkSum;
                        *pLong--         = '\0';
                    } else {
                        assert(DirEnt.L_ChkSum == long_name_chksum,
                               "Check sum unmatch.");
                    }
                    // copy name
                    for (int ni = 4 - 2; ni >= 0; ni -= 2) {
                        char ch = DirEnt.L_Name3[ni];
                        if (ch != '\0' && ch != 0xFF)
                            *pLong-- = ch;
                    }

                    for (int ni = 12 - 2; ni >= 0; ni -= 2) {
                        char ch = DirEnt.L_Name2[ni];
                        if (ch != '\0' && ch != 0xFF)
                            *pLong-- = ch;
                    }

                    for (int ni = 10 - 2; ni >= 0; ni -= 2) {
                        char ch = DirEnt.L_Name1[ni];
                        if (ch != '\0' && ch != 0xFF)
                            *pLong-- = ch;
                    }
                }
                if (DirEnt.Attr & ATTR_VOLUME_ID)
                    continue; // TODO: read it and save it

                if (long_name) {
                    // if there is a lone name ent
                    // Checksum
                    assert(long_name_chksum == checksum_fname(DirEnt.Name),
                           "Check sum failed.");
                    // call callback
                    if (callback(&DirEnt, pLong + 1, data) != 0) {
                        bio_cache_release(buf);
                        return;
                    }
                    kfree(long_name);
                    long_name = pLong = NULL;
                    long_name_chksum  = 0;
                } else {
                    char dname[12] = {[0 ... 11] = 0};
                    read_8_3_filename(DirEnt.Name, dname);
                    // call callback
                    if (callback(&DirEnt, dname, data) != 0) {
                        bio_cache_release(buf);
                        return;
                    }
                }
            }
            bio_cache_release(buf);
        }
        dir_clus = get_next_clus_in_FAT(fs, dir_clus);
        if (dir_clus >= 0xFFFFFF8 && dir_clus <= 0xFFFFFFF)
            break;
    }
}

// 在目录项中寻找名字为name的目录项，并写入found_inode中
static int lookup(inode_t *dir, const char *name, dentry_t **found_inode) {}
// 链接/取消链接 一个inode到dir里。
static int link(inode_t *inode, inode_t *dir, const char *name) {}
static int unlink(inode_t *dir, const char *name) {}
// 创建/删除目录inode
static int mkdir(inode_t *parent, const char *name, inode_t **dir) {}
static int rmdir(inode_t *parent, const char *name, inode_t **dir) {}

#define IS_LEAP_YEAR(year)                                                     \
    (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0))
static uint64_t fat_date2ts(uint16_t date) {
    int day   = date & 0x1F;
    int month = (date >> 5) & 0xF;
    int year  = (date >> 9) & 0x7F; // start from 1980
    year += 1980;
    // unix ts start from 1970-01-01
    const int  a_day_in_sec        = 60 * 60 * 24;
    static int days_till_a_month[] = {
        0,
        0,
        31,
        31 + 28,
        31 + 28 + 31,
        31 + 28 + 31 + 30,
        31 + 28 + 31 + 30 + 31,
        31 + 28 + 31 + 30 + 31 + 30,
        31 + 28 + 31 + 30 + 31 + 30 + 31,
        31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
        31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
        31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
        31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
    };
    int days = day - 1 + days_till_a_month[month];
    if (month > 2 && IS_LEAP_YEAR(year))
        days += 1;
    for (int i = 1970; i < year; i++)
        days += IS_LEAP_YEAR(i) ? 366 : 365;
    return days * a_day_in_sec;
}

static uint64_t fat_time2ts(uint16_t time) {
    int sec  = (time & 0x1F) * 2;
    int min  = (time >> 5) & 0x3F;
    int hour = (time >> 11) & 0x1F;
    return sec + min * 60 + hour * 60 * 60;
}

static int read_dir(inode_t *dir, read_dir_callback callback, void *data) {
    assert(dir->i_type == inode_dir, "Must be dir.");
    struct FAT32_FileSystem *fs = dir->i_sb->s_fs_data;
    uint32_t dir_clus = ((fatfs_inode_data_t *)dir->i_fs_data)->start_clus;

    int looper(union FAT32_DirEnt * dirent, char *long_name, void *_data) {
        inode_t *inode = alloc_inode(dir->i_sb);
        ((fatfs_inode_data_t *)inode->i_fs_data)->start_clus =
            dirent->FstClusHI << 16 | dirent->FstClusLO;
        inode->i_size   = dirent->FileSize;
        inode->i_nlinks = 1;

        dentry_t *dentry = (dentry_t *)kmalloc(sizeof(dentry_t));
        memset(dentry, 0, sizeof(dentry_t));
        dentry->d_subdirs = (list_head_t)LIST_HEAD_INIT(dentry->d_subdirs);
        dentry->d_inode   = inode;
        if (strlen(long_name) <= D_NAME_LEN - 1)
            strcpy(dentry->d_name, long_name);
        else {
            memcpy(dentry->d_name, long_name, 31);
            dentry->d_name[D_NAME_LEN - 1] =
                '\0'; // bad truncate, consider use kmamlloc
        }

        inode->i_mtime =
            fat_date2ts(dirent->WrtDate) + fat_time2ts(dirent->WrtTime);
        inode->i_ctime =
            fat_date2ts(dirent->CrtDate) + fat_time2ts(dirent->CrtTime);
        inode->i_atime = fat_date2ts(dirent->LastAccDate);

        if (dirent->Attr & ATTR_DIR) {
            // ent is a dir
            inode->i_type  = inode_dir;
            dentry->d_type = D_TYPE_DIR;
        } else {
            // ent is a file
            inode->i_type  = inode_file;
            dentry->d_type = D_TYPE_FILE;
        }

        callback(dentry, data);
        return 0;
    };

    loop_fat_dirent(fs, dir_clus, looper, NULL);

    return 0;
}

static inode_ops_t inode_ops = {
    .mkdir    = mkdir,
    .rmdir    = rmdir,
    .link     = link,
    .unlink   = unlink,
    .lookup   = NULL,
    .read_dir = read_dir,
};

static inode_t *alloc_inode(superblock_t *sb) {
    inode_t *inode = (inode_t *)kmalloc(sizeof(inode_t));
    assert(inode, "Out of memory.");
    memset(inode, 0, sizeof(inode_t));
    memcpy(inode->i_dev, sb->s_dev, sizeof(uint16_t) * 2);
    inode->i_sb   = sb;
    inode->i_op   = &inode_ops;
    inode->i_ino  = inode_idx;
    inode->i_f_op = &file_ops;

    fatfs_inode_data_t *i_data =
        (fatfs_inode_data_t *)kmalloc(sizeof(fatfs_inode_data_t));
    memset(i_data, 0, sizeof(fatfs_inode_data_t));
    inode->i_fs_data = (void *)i_data;

    fatfs_inode_index_t *i_index =
        (fatfs_inode_index_t *)kmalloc(sizeof(fatfs_inode_index_t));
    memset(i_index, 0, sizeof(fatfs_inode_index_t));
    i_index->inode       = inode;
    i_index->rb_node.key = inode_idx;
    rb_insert(&inode_tree, &i_index->rb_node); // no collsion possible
    inode_idx++;

    return inode;
}

static int free_inode(superblock_t *sb, inode_t *inode) {}

static int write_inode(superblock_t *sb, inode_t *inode) {}

static int read_inode(superblock_t *sb, inode_t *inode) {}

static superblock_ops_t s_op = {
    .write_inode = write_inode,
    .read_inode  = read_inode,
    .alloc_inode = alloc_inode,
    .free_inode  = free_inode,
};

superblock_t *do_mount_fatfs(superblock_t *sb, inode_t *dev, void *flags) {
    sb->s_op = &s_op;
    // read superblock
    struct FAT32_FileSystem *fatfs =
        (struct FAT32_FileSystem *)kmalloc(sizeof(struct FAT32_FileSystem));
    fat_read_superblock(dev->i_dev[1], fatfs);
    sb->s_fs_data = (void *)fatfs;
    // load root
    inode_t *root = alloc_inode(sb);
    root->i_type  = inode_dir;

    ((fatfs_inode_data_t *)(root->i_fs_data))->start_clus = fatfs->RootClus;

    sb->s_root = root;
    return sb;
}

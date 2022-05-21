//
// Created by shiroko on 22-5-22.
//

#ifndef __SYS_STRUCTS_H__
#define __SYS_STRUCTS_H__

#include <types.h>

struct dirent {
    uint64_t       d_ino;    // 索引结点号
    uint64_t       d_off;    // 到下一个dirent的偏移
    unsigned short d_reclen; // 当前dirent的长度
    unsigned char  d_type;   // 文件类型
    char           d_name[]; // 文件名
};

struct kstat {
    uint16_t      st_dev;
    uint32_t      st_ino;
    int           st_mode;
    int           st_nlink;
    int           st_uid;
    int           st_gid;
    uint16_t      st_rdev;
    unsigned long __pad;
    size_t        st_size;
    size_t        st_blksize;
    int           __pad2;
    size_t        st_blocks;
    long          st_atime_sec;
    long          st_atime_nsec;
    long          st_mtime_sec;
    long          st_mtime_nsec;
    long          st_ctime_sec;
    long          st_ctime_nsec;
    unsigned      __unused[2];
};

struct tms {
    uint64_t tms_utime;  /* user time */
    uint64_t tms_stime;  /* system time */
    uint64_t tms_cutime; /* user time of children */
    uint64_t tms_cstime; /* system time of children */
};

#define SYS_NMLN 257
struct utsname {
    char sysname[SYS_NMLN];  /* Operating system name (e.g., "Linux") */
    char nodename[SYS_NMLN]; /* Name within "some implementation-defined
                      network" */
    char release[SYS_NMLN];  /* Operating system release
                      (e.g., "2.6.28") */
    char version[SYS_NMLN];  /* Operating system version */
    char machine[SYS_NMLN];  /* Hardware identifier */
#ifdef _GNU_SOURCE
    char domainname[SYS_NMLN]; /* NIS or YP domain name */
#endif
};

struct timespec {
    uint64_t tv_sec;  /* 秒 */
    long     tv_nsec; /* 纳秒, 范围在0~999999999 */
};

#endif // __SYS_STRUCTS_H__
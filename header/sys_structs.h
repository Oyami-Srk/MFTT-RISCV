//
// Created by shiroko on 22-5-22.
//

#ifndef __SYS_STRUCTS_H__
#define __SYS_STRUCTS_H__

#include <types.h>

struct tms {
    uint64_t tms_utime;  /* user time */
    uint64_t tms_stime;  /* system time */
    uint64_t tms_cutime; /* user time of children */
    uint64_t tms_cstime; /* system time of children */
};

#define SYS_NMLN 65
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
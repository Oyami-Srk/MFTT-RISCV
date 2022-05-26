//
// Created by shiroko on 22-4-28.
//

#ifndef __CONFIGS_H__
#define __CONFIGS_H__

#define MAX_PROC   32
#define MAX_CPUS   8
#define MAX_DEV_ID 32

#define DEV_TTY         1
#define DEV_VIRTIO_DISK 2
#define DEV_BUFFERED_IO 5

// FIXME: BAD HARD CODE
#ifdef PLATFORM_QEMU
#define USE_SOFT_INT_COMP 0
#else
#define USE_SOFT_INT_COMP 1
#endif
#define HART_COUNT 2

#endif // __CONFIGS_H__
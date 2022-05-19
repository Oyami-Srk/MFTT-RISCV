//
// Created by shiroko on 22-5-20.
//

#include <driver/console.h>
#include <environment.h>
#include <lib/stdlib.h>
#include <lib/sys/spinlock.h>
#include <syscall.h>
#include <vfs.h>

sysret_t sys_test(struct trap_context *trapframe) {

    extern void virtio_disk_rw(uint64_t addr, char *buf, size_t size, int func);
    char        buffer[512];
    virtio_disk_rw(0x0, buffer, 512, 0); // read test
    kprintf("Read!!!\n");

    return 0;
}

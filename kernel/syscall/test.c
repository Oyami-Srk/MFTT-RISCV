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
    dentry_t *test = vfs_get_dentry("/mnt/TEST_DIR/C", NULL);
}

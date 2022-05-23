//
// Created by shiroko on 22-5-6.
//

#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

int main() {
    syscall_test(0, 0);
    printf("Hello world.!\n");
    int fd_disk = openat(0, "/dev/vda", 0, 0);
    if (fd_disk == -1)
        printf("Disk /dev/vda not found.\n");
    else {
        char buf[32];
        lseek(fd_disk, 119, 0);
        read(fd_disk, buf, 0x20);
        buf[31] = '\0';
        printf("lseek and read /dev/vda test: %s\n", buf);
    }
    printf("Mount test.\n");
    int parent_fd = openat(0, "/", 0, 0);
    int mnt       = mkdirat(parent_fd, "mnt", 0);
    if (mnt)
        mount("/dev/vda", "/mnt", "fat32", 0, NULL);
    else
        printf("Cannot create mnt dir");
    for (int i = 0; i <= 3; i++) {
        printf("ticks: %d.\n", ticks());
        sleep(2);
    }
    return 0;
}
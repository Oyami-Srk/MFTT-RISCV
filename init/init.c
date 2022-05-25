//
// Created by shiroko on 22-5-6.
//

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

#define STAT_MAX_NAME 32

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

    char      buffer[64];
    dirent_t *dent = (dirent_t *)buffer;
    /*
    int       cwdfd = openat(AT_FDCWD, ".", 0, 0);
    printf("test ls.\n");
    while (getdents64(cwdfd, dent, 32) != 0) {
        printf("%c: %s\n", dent->d_type, dent->d_name);
    }*/

    printf("Mount test.\n");
    int parent_fd = openat(AT_FDCWD, "/", 0, 0);
    if (parent_fd < 0) {
        printf("Failed open root.\n");
    } else {
        if (mkdirat(parent_fd, "mnt", 0) != 0) {
            printf("Failed to mkdir /mnt.\n");
        } else {
            int mnt = openat(AT_FDCWD, "/mnt", 0, 0);
            if (mnt) {
                if (mount("/dev/vda", "/mnt", "fat32", 0, NULL) != 0)
                    printf("Cannot mount /dev/vda to /mnt.\n");
                else {
                    printf("test ls /mnt.\n");
                    while (getdents64(mnt, dent, 64) != 0) {
                        printf("%c: %s\n", dent->d_type, dent->d_name);
                    }
                    printf("test open file.\n");
                    int disk_fd = openat(mnt, "HELLO.TXT", 0, 0);
                    if (disk_fd < 0)
                        printf("Open HELLO.TXT failed.\n");
                    else {
                        printf("Open HELLO.TXT successful.\n");
                        char buf[32];
                        int  bytes = read(disk_fd, buf, 0x20);
                        buf[31]    = '\0';
                        printf("Read %d bytes from HELLO.TXT: %s\n", bytes,
                               buf);
                        close(disk_fd);
                    }
                    printf("test execve.\n");
                    const char *argv[] = {"arg1", "pworld", "helloi", "arg4",
                                          NULL};
                    const char *env[]  = {"env1", "env2", NULL};
                    execve("/mnt/PROG1", (char *const *)argv,
                           (char *const *)env);
                }
            } else
                printf("Cannot create mnt dir");
        }
    }
    for (int i = 0; i <= 3; i++) {
        printf("ticks: %d.\n", ticks());
        sleep(2);
    }
    return 0;
}
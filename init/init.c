//
// Created by shiroko on 22-5-6.
//

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

#define STAT_MAX_NAME 32

#ifdef PLATFORM_QEMU
#define DISK_DEVICE "/dev/vda"
#else
#define DISK_DEVICE "/dev/sda"
#endif

#if 0
int main() {
    printf("Hello world.!\n");
    int fd_disk = openat(0, DISK_DEVICE, 0, 0);
    if (fd_disk == -1)
        printf("Disk " DISK_DEVICE " not found.\n");
    else {
        char buf[32];
        lseek(fd_disk, 119, 0);
        read(fd_disk, buf, 0x20);
        buf[31] = '\0';
        printf("lseek and read " DISK_DEVICE " test: %s\n", buf);
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
        printf("Parent fd: %d.\n", parent_fd);
        if (mkdirat(parent_fd, "mnt", 0) != 0) {
            printf("Failed to mkdir /mnt.\n");
        } else {
            int mnt = openat(AT_FDCWD, "/mnt", 0, 0);
            if (mnt) {
                if (mount(DISK_DEVICE, "/mnt", "fat32", 0, NULL) != 0)
                    printf("Cannot mount " DISK_DEVICE " to /mnt.\n");
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
                    syscall_test(0, 0);
                    printf("test fork, pipe and execve.\n");
                    int fds[2] = {0, 0};
                    if (pipe2(fds) == 0) {
                        printf("Pipe create successful.\n");
                    } else {
                        fds[0] = fds[1] = 0;
                    }
                    int ret = fork();
                    if (ret == 0) {
                        if (fds[0] != 0) {
                            printf("I'm child, waiting for pipe to read.\n");
                            char pipe_buf[32];
                            memset(pipe_buf, 0, 32);
                            read(fds[0], pipe_buf, 32);
                            printf("Pipe read: %s\n", pipe_buf);
                        }
                        printf("I'm child, doing execve right now.\n");
                        const char *argv[] = {"arg1", "pworld", "helloi",
                                              "arg4", NULL};
                        const char *env[]  = {"env1", "env2", NULL};
                        execve("/mnt/PROG1", (char *const *)argv,
                               (char *const *)env);
                    } else if (ret > 0) {
                        printf("I'm parent, child pid is %d.\n", ret);
                        if (fds[1] != 0) {
                            printf("I'm parent, write to pipe.\n");
                            char pipe_buf[32];
                            strcpy(pipe_buf, "Hello pipe, this is parent!");
                            write(fds[1], pipe_buf, 32);
                        }
                        printf("Wating for exit...\n");
                        int      status;
                        uint64_t pid = wait4(WAIT_ANY, &status, 0);
                        printf("PID %d exited with status %d.\n", pid, status);
                    } else {
                        printf("Error code: %d.\n", ret);
                    }
                }
            } else
                printf("Cannot create mnt dir");
        }
    }
    for (int i = 0; i <= 30000; i++) {
        printf("ticks: %d.\n", ticks());
        sleep(2);
    }
    return 0;
}
#endif

static inline char char_upper(char c) {
    return c - (c >= 'a' && c <= 'z' ? 32 : 0);
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

void test_execve(const char *name, bool *meet) {
    char *target = "getcwd";
    if (*meet != true) {
        if (strcmp(name, target) == 0)
            *meet = true;
        else
            return;
    }
    int ret = fork();
    if (ret == 0) {
        char name_buffer[32];
        write_8_3_filename((char *)name, name_buffer);
        for (char *p = name_buffer; *p != '\0'; p++)
            if (*p == ' ') {
                *p = '\0';
                break;
            }
        // child
        const char *argv[] = {name, NULL};
        const char *env[]  = {NULL};
        printf("Try execve %s (FAT Name: %s).\n", name, name_buffer);
        int r = execve(name_buffer, (char *const *)argv, (char *const *)env);
        printf("Execve failed: %d.\n", r);
        exit(r);
    } else if (ret > 0) {
        // parent
        int status;
        int pid = wait4(WAIT_ANY, &status, 0);
        printf("PID %d exited with status %d.\n", pid, status);
    } else {
        printf("!!!!Error while fork.\n");
    }
}

int main() {
    char      buffer[512];
    char      dent_buffer[64];
    dirent_t *dent = (dirent_t *)dent_buffer;
    bool      meet = false;
    // TODO: exec a shell and initscript. IMPORTANT!!
    int parent_fd = openat(AT_FDCWD, "/", 0, 0);
    if (parent_fd < 0) {
        printf("Failed open root.\n");
    } else {
        if (mkdirat(parent_fd, "mnt", 0) != 0) {
            printf("Failed to mkdir /mnt.\n");
        } else {
            int mnt = openat(AT_FDCWD, "/mnt", 0, 0);
            if (mnt) {
                if (mount(DISK_DEVICE, "/mnt", "fat32", 0, NULL) != 0)
                    printf("Cannot mount " DISK_DEVICE " to /mnt.\n");
                else {
                    printf("List /mnt:\n");
                    while (getdents64(mnt, dent, 64) != 0) {
                        printf("%c: %s\n", dent->d_type, dent->d_name);
                    }
                    printf("\nList /mnt/riscv64:\n");
                    chdir("/mnt/RISCV64");
                    int cwd = openat(AT_FDCWD, ".", 0, 0);
                    while (getdents64(cwd, dent, 64) != 0) {
                        printf("%c: %s\n", dent->d_type, dent->d_name);
                    }
                    int disk_fd = openat(AT_FDCWD, "RUN-ALL.SH", 0, 0);
                    if (disk_fd < 0)
                        printf("Open RUN_ALL.SH failed.\n");
                    else {
                        int   bytes = read(disk_fd, buffer, 512);
                        char  line[32];
                        char *p = line;
                        for (int i = 0; i <= bytes; i++) {
                            if (buffer[i] == '\n') {
                                *p = '\0';
                                if (!(line[0] == '#' || line[0] == ' ' ||
                                      line[0] == '\0')) {
                                    if (line[0] == '"')
                                        break;
                                    if (line[strlen(line) - 1] != '"') {
                                        if (strcmp(".sh", line + strlen(line) -
                                                              3) == 0) {
                                            printf("Shell script.\n");
                                        } else {
                                            test_execve(line, &meet);
                                        }
                                    }
                                }
                                p = line;
                            } else if (buffer[i] == '\0') {
                                break;
                            } else {
                                *p++ = buffer[i];
                            }
                        }
                    }
                }
            }
        }
    }
    for (;;)
        sleep(1000000000);
    return 0;
}
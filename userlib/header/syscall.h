//
// Created by shiroko on 22-5-6.
//

#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stddef.h>
#include <sys_structs.h>
#include <syscall_nums.h>
#include <types.h>

typedef uint64_t sysret_t;

uint64_t ticks();
void     print(char *buffer);
void     sleep(uint64_t ticks);

int    openat(int pfd, const char *filename, int flags, int mode);
int    close(int fd);
int    write(int fd, char *buf, size_t count);
int    read(int fd, const char *buf, size_t count);
size_t lseek(int fd, size_t offset, int whence);
char  *getcwd(char *buf, size_t len);
int    pipe2(int fd[2]);
int    dup(int fd);
int    dup3(int old, int new);
int    chdir(const char *path);
size_t getdents64(int fd, dirent_t *buf, size_t len);
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath,
           int flags);
int unlinkat(int dirfd, const char *path, int flags);
int mkdirat(int dirfd, const char *path, int mode);
int umount2(const char *path, int flags);
int mount(const char *dev, const char *dir, const char *fstype, uint64_t flags,
          const void *data);
int fstat(int fd, kstat_t *kst);
int clone(int flags, char *stack, int ptid, int tls, int ctid);
int fork();
int execve(const char *path, char *const argv[], char *const envp[]);
int wait4(int pid, int *status, int options);
void      exit(int ec);
int       getppid();
int       getpid();
uintptr_t brk(uintptr_t brk);
int       munmap(void *start, size_t len);
uintptr_t mmap(void *start, size_t len, int prot, int fd, size_t offset);
uint64_t  times(struct tms *tms);
int       uname(struct utsname *uts);
int       sched_yield();
int       gettimeofday(struct timespec *ts);
int       nanosleep(struct timespec *req, struct timespec *rem);

// For development test
int syscall_test(uint64_t a1, uint64_t a2);

#endif // __SYSCALL_H__
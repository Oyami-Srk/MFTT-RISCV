//
// Created by shiroko on 22-5-6.
//

#include <syscall.h>

#define USED __attribute__((used))

static inline sysret_t __internal_syscall(long n, uint64_t _a0, uint64_t _a1,
                                          uint64_t _a2, uint64_t _a3,
                                          uint64_t _a4, uint64_t _a5) {
    register uint64_t a0 asm("a0")         = _a0;
    register uint64_t a1 asm("a1")         = _a1;
    register uint64_t a2 asm("a2")         = _a2;
    register uint64_t a3 asm("a3")         = _a3;
    register uint64_t a4 asm("a4")         = _a4;
    register uint64_t a5 asm("a5")         = _a5;
    register long     syscall_id asm("a7") = n;
    asm volatile("ecall"
                 : "+r"(a0)
                 : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                   "r"(syscall_id));
    return a0;
}

#define SYSCALL_0(id)     __internal_syscall(id, 0, 0, 0, 0, 0, 0)
#define SYSCALL_1(id, a0) __internal_syscall(id, (uint64_t)(a0), 0, 0, 0, 0, 0)
#define SYSCALL_2(id, a0, a1)                                                  \
    __internal_syscall(id, (uint64_t)(a0), (uint64_t)(a1), 0, 0, 0, 0)
#define SYSCALL_3(id, a0, a1, a2)                                              \
    __internal_syscall(id, (uint64_t)(a0), (uint64_t)(a1), (uint64_t)(a2), 0,  \
                       0, 0)
#define SYSCALL_4(id, a0, a1, a2, a3)                                          \
    __internal_syscall(id, (uint64_t)(a0), (uint64_t)(a1), (uint64_t)(a2),     \
                       (uint64_t)(a3), 0, 0)
#define SYSCALL_5(id, a0, a1, a2, a3, a4)                                      \
    __internal_syscall(id, (uint64_t)(a0), (uint64_t)(a1), (uint64_t)(a2),     \
                       (uint64_t)(a3), (uint64_t)(a4), 0)
#define SYSCALL_6(id, a0, a1, a2, a3, a4, a5)                                  \
    __internal_syscall(id, (uint64_t)(a0), (uint64_t)(a1), (uint64_t)(a2),     \
                       (uint64_t)(a3), (uint64_t)(a4), (uint64_t)(a5))

#define SYSCALL_X(x, id, a0, a1, a2, a3, a4, a5, FUNC, ...) FUNC

#define SYSCALL(...)                                                           \
    SYSCALL_X(, ##__VA_ARGS__, SYSCALL_6(__VA_ARGS__), SYSCALL_5(__VA_ARGS__), \
              SYSCALL_4(__VA_ARGS__), SYSCALL_3(__VA_ARGS__),                  \
              SYSCALL_2(__VA_ARGS__), SYSCALL_1(__VA_ARGS__),                  \
              SYSCALL_0(__VA_ARGS__))

uint64_t ticks() {
    //    return (uint64_t)__internal_syscall(SYS_ticks, 0, 0, 0, 0, 0, 0, 0);
    return (uint64_t)SYSCALL(SYS_ticks);
}
void print(char *buffer) { SYSCALL(SYS_print, (uint64_t)buffer); }

void sleep(uint64_t ticks) { SYSCALL(SYS_sleep, ticks); }

int openat(int pfd, const char *filename, int flags, int mode) {
    return SYSCALL(SYS_openat, pfd, (uintptr_t)filename, flags, mode);
}
int close(int fd) { return SYSCALL(SYS_close, fd); }
int write(int fd, char *buf, size_t count) {
    return SYSCALL(SYS_write, fd, (uintptr_t)buf, count);
}
int read(int fd, const char *buf, size_t count) {
    return SYSCALL(SYS_read, fd, (uintptr_t)buf, count);
}

size_t lseek(int fd, size_t offset, int whence) {
    return SYSCALL(SYS_lseek, fd, offset, whence);
}
char *getcwd(char *buf, size_t len) {
    return (char *)SYSCALL(SYS_getcwd, buf, len);
}
int    pipe2(int fd[2]) { return SYSCALL(SYS_pipe2, fd); }
int    dup(int fd) { return SYSCALL(SYS_dup, fd); }
int    dup3(int old, int new) { return SYSCALL(SYS_dup3, old, new); }
int    chdir(const char *path) { return SYSCALL(SYS_chdir, path); }
size_t getdents64(int fd, struct dirent *buf, size_t len) {
    return SYSCALL(SYS_getdents64, fd, buf, len);
}
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath,
           int flags) {
    return SYSCALL(SYS_linkat, olddirfd, oldpath, newdirfd, newpath, flags);
}
int unlinkat(int dirfd, const char *path, int flags) {
    return SYSCALL(SYS_unlinkat, dirfd, path, flags);
}
int mkdirat(int dirfd, const char *path, int mode) {
    return SYSCALL(SYS_mkdirat, dirfd, path, mode);
}
int umount2(const char *path, int flags) {
    return SYSCALL(SYS_umount2, path, flags);
}
int mount(const char *dev, const char *dir, const char *fstype, uint64_t flags,
          const void *data) {
    return SYSCALL(SYS_mount, dev, dir, fstype, flags, data);
}
int fstat(int fd, struct kstat *kst) { return SYSCALL(SYS_fstat, fd, kst); }
int clone(int flags, char *stack, int ptid, int tls, int ctid) {
    return SYSCALL(SYS_clone, flags, stack, ptid, tls, ctid);
}
int execve(const char *path, char *const argv[], char *const envp[]) {
    return SYSCALL(SYS_execve, path, argv, envp);
}
int wait4(int pid, int *status, int options) {
    return SYSCALL(SYS_wait4, pid, status, options);
}
void      exit(int ec) { SYSCALL(SYS_exit, ec); }
int       getppid() { return SYSCALL(SYS_getppid); }
int       getpid() { return SYSCALL(SYS_getpid); }
uintptr_t brk(uintptr_t _brk) { return SYSCALL(SYS_brk, _brk); }
int munmap(void *start, size_t len) { return SYSCALL(SYS_munmap, start, len); }
uintptr_t mmap(void *start, size_t len, int prot, int fd, size_t offset) {
    return SYSCALL(SYS_mmap, start, len, prot, fd, offset);
}
uint64_t times(struct tms *tms) { return SYSCALL(SYS_times, tms); }
int      uname(struct utsname *uts) { return SYSCALL(SYS_uname, uts); }
int      sched_yield() { return SYSCALL(SYS_sched_yield); }
int gettimeofday(struct timespec *ts) { return SYSCALL(SYS_gettimeofday, ts); }
int nanosleep(struct timespec *req, struct timespec *rem) {
    return SYSCALL(SYS_nanosleep, req, rem);
}

// For developments tests.
int syscall_test(uint64_t a1, uint64_t a2) { return SYSCALL(SYS_test, a1, a2); }

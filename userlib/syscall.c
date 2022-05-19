//
// Created by shiroko on 22-5-6.
//

#include <syscall.h>

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

#define SYSCALL_0(id)             __internal_syscall(id, 0, 0, 0, 0, 0, 0)
#define SYSCALL_1(id, a0)         __internal_syscall(id, a0, 0, 0, 0, 0, 0)
#define SYSCALL_2(id, a0, a1)     __internal_syscall(id, a0, a1, 0, 0, 0, 0)
#define SYSCALL_3(id, a0, a1, a2) __internal_syscall(id, a0, a1, a2, 0, 0, 0)
#define SYSCALL_4(id, a0, a1, a2, a3)                                          \
    __internal_syscall(id, a0, a1, a2, a3, 0, 0)
#define SYSCALL_5(id, a0, a1, a2, a3, a4)                                      \
    __internal_syscall(id, a0, a1, a2, a3, a4, 0)
#define SYSCALL_6(id, a0, a1, a2, a3, a4, a5)                                  \
    __internal_syscall(id, a0, a1, a2, a3, a4, a5)

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

// For developments tests.
int syscall_test(uint64_t a1, uint64_t a2) { return SYSCALL(SYS_test, a1, a2); }

#include <stdlib.h>
#include <syscall.h>

int main() {
    int fd = openat(0, "/dev/tty", 0, 0);
    write(fd, "Hello My First Program execve from disk!\n", 12);
    for (;;) {
        sleep(100000);
    }
}

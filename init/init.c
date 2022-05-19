//
// Created by shiroko on 22-5-6.
//

#include <stdlib.h>
#include <syscall.h>

int main() {
    print("Hello world.\n");
    syscall_test(0, 0);
    int fd = openat(0, "/dev/tty", 0, 0);
    write(fd, "Hello File!\n", 12);
    char buf[32];
    for (;;) {
        itoa(ticks(), buf, 10);
        print(buf);
        print("\n");
        sleep(2);
    }
}
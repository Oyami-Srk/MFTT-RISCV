#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

int main() {
    printf("Hello My First Program execve from disk!\n");
    for (;;) {
        sleep(100000);
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

int main() {
    printf("Hello My First Program execve from disk!\n");
    printf("And then I do exit.\n");
    exit(233);
    return 0;
}

//
// Created by shiroko on 22-5-22.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

int printf(const char *fmt, ...) {
    int         i;
    static char buf[1024];
    va_list     arg;
    va_start(arg, fmt);
    i      = vsprintf(buf, fmt, arg);
    buf[i] = 0;
    write(1, buf, strlen(buf));
    return i;
}

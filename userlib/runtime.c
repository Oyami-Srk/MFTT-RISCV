//
// Created by shiroko on 22-6-2.
//

#include <syscall.h>

/*
// stack protector
uintptr_t __stack_chk_guard = 0x20010125beef5a5a;

__attribute__((noreturn)) void __stack_chk_fail(void) {
    write(2, "Stack overflow detected...", 27);
    exit(-1);
    while (1)
        ;
}
 */
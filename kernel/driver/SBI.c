//
// Created by shiroko on 22-4-18.
//
#include <driver/SBI.h>

#if FALSE
uint64_t SBI_call(uint64_t ID, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                  uint64_t arg4) {
    uint64_t ret;
    asm volatile("mv x10, %1\n\t"
                 "mv x11, %2\n\t"
                 "mv x12, %3\n\t"
                 "mv x13, %4\n\t"
                 "mv x17, %5\n\t"
                 "ecall\n\t"
                 "mv %0, x10\n"
                 : "=r"(ret)
                 : "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "r"(ID)
                 : "x10", "x11", "x12", "x17");
    return ret;
}

#endif
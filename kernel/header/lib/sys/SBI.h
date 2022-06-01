//
// Created by shiroko on 22-4-18.
//

#ifndef __SBI_H__
#define __SBI_H__

#include <types.h>

#define SBI_SET_TIMER 0
#define SBI_PUTCHAR   1
#define SBI_GETCHAR   2
#define SBI_CLEAR_IPI 3
#define SBI_SEND_IPI  4
#define SBI_SHUTDOWN  8

#define SBI_EXT_SRST            0x53525354
#define SBI_EXT_SRST_FUNC_RESET 0x0

// RustSBI for K210
#define SBI_SET_MIE 0x0A000005

// uint64_t SBI_call(uint64_t ID, uint64_t arg1, uint64_t arg2, uint64_t arg3);

#define SBI_call(which, arg0, arg1, arg2, arg3)                                \
    ({                                                                         \
        register uintptr_t a0 asm("a0") = (uintptr_t)(arg0);                   \
        register uintptr_t a1 asm("a1") = (uintptr_t)(arg1);                   \
        register uintptr_t a2 asm("a2") = (uintptr_t)(arg2);                   \
        register uintptr_t a3 asm("a3") = (uintptr_t)(arg3);                   \
        register uintptr_t a7 asm("a7") = (uintptr_t)(which);                  \
        asm volatile("ecall"                                                   \
                     : "+r"(a0)                                                \
                     : "r"(a1), "r"(a2), "r"(a3), "r"(a7)                      \
                     : "memory");                                              \
        a0;                                                                    \
    })

#define SBI_EXT_call(ext, func, arg0, arg1, arg2, arg3)                        \
    ({                                                                         \
        register uintptr_t a0 asm("a0") = (uintptr_t)(arg0);                   \
        register uintptr_t a1 asm("a1") = (uintptr_t)(arg1);                   \
        register uintptr_t a2 asm("a2") = (uintptr_t)(arg2);                   \
        register uintptr_t a3 asm("a3") = (uintptr_t)(arg3);                   \
        register uintptr_t a6 asm("a6") = (uintptr_t)(func);                   \
        register uintptr_t a7 asm("a7") = (uintptr_t)(ext);                    \
        asm volatile("ecall"                                                   \
                     : "+r"(a0)                                                \
                     : "r"(a1), "r"(a2), "r"(a3), "r"(a6), "r"(a7)             \
                     : "memory");                                              \
        a0;                                                                    \
    })

static inline void SBI_putchar(char x) { SBI_call(SBI_PUTCHAR, x, 0, 0, 0); }
static inline uint64_t SBI_getchar(void) {
    return SBI_call(SBI_GETCHAR, 0, 0, 0, 0);
}
static inline void SBI_shutdown() { SBI_call(SBI_SHUTDOWN, 0, 0, 0, 0); }
static inline void SBI_set_timer(uint64_t timer_value) {
    SBI_call(SBI_SET_TIMER, timer_value, 0, 0, 0);
}
// TODO: Check latest opensbi doc, this is replaced by a new send_ipi
static inline void SBI_send_ipi(uint64_t *hart_mask) {
    SBI_call(SBI_SEND_IPI, hart_mask, 0, 0, 0);
}

static inline void SBI_set_mie() { SBI_call(SBI_SET_MIE, 0, 0, 0, 0); }
static inline void SBI_ext_srst() {
    SBI_EXT_call(SBI_EXT_SRST, SBI_EXT_SRST_FUNC_RESET, 0, 0, 0, 0);
}

#endif // __SBI_H__
//
// Created by shiroko on 22-5-2.
//

#ifndef __TRAP_UTILS_H__
#define __TRAP_UTILS_H__

#include <lib/stdlib.h>
#include <riscv.h>
#include <trap.h>

#ifdef PLATFORM_QEMU
#define COULD_BE_PAGEFAULT(x) ((x) == 15 || (x) == 13 || (x) == 12 || (x) == 5)
#else
#define COULD_BE_PAGEFAULT(x)                                                  \
    ((x) == 15 || (x) == 13 || (x) == 12 || (x) == 7 || (x) == 5 || (x) == 1)
#endif

// 其实用vector命名不太正确，因为这里用DIRECT MODE不是向量表模式
// 沿用OmochaOS的命名习惯
// FIXME: rename
extern void supervisor_interrupt_vector(); // trap.S
extern void user_interrupt_vector();       // trap.S

static inline void set_interrupt_to(uintptr_t vector) {
    assert((((uintptr_t)vector) & 0x3) == 0,
           "Interrupt Vector must be 4bytes aligned.");
    // stvec: Supervisor Trap-Vector Base Address, low two bits are mode.
    CSR_Write(stvec, ((uintptr_t)vector & (~0x3)) | TRAP_MODE);
}

static inline void set_interrupt_to_kernel() {
    set_interrupt_to((uintptr_t)supervisor_interrupt_vector);
}

static inline void set_interrupt_to_user() {
    set_interrupt_to((uintptr_t)user_interrupt_vector);
}

void exception_panic(uint64_t scause, uint64_t stval, uint64_t sepc,
                     uint64_t sstatus, struct trap_context *trapframe);

#endif // __TRAP_UTILS_H__
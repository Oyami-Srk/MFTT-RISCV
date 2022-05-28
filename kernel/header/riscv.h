#ifndef __RISCV_H__
#define __RISCV_H__

#include <types.h>

#define ALWAYS_INLINE __attribute__((always_inline))

// tp register, we use it for saving cpuid(hartid)
static ALWAYS_INLINE inline void w_tp(uint64_t hartid) {
    asm volatile("mv tp, %0" ::"r"(hartid));
}

static ALWAYS_INLINE inline uint64_t r_tp() {
    uint64_t ret;
    asm volatile("mv %0, tp" : "=r"(ret)::);
    return ret;
}

#define CSR_Write(reg, value) asm volatile("csrw " #reg ", %0" ::"r"((value)))
#define CSR_Read(reg)                                                          \
    ({                                                                         \
        static unsigned long long __v;                                         \
        asm volatile("csrr %0, " #reg : "=r"((__v)));                          \
        __v;                                                                   \
    })
#define CSR_RWOR(reg, value)  CSR_Write(reg, CSR_Read(reg) | (value))
#define CSR_RWAND(reg, value) CSR_Write(reg, CSR_Read(reg) & (value))

// Supervisor Status Register, sstatus
#define SSTATUS_UIE      (1L << 0) // User Interrupt Enable
#define SSTATUS_SIE      (1L << 1) // Supervisor Interrupt Enable
#define SSTATUS_UPIE     (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SPIE     (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_SPP      (1L << 8) // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_FS0      (1L << 13)
#define SSTATUS_FS1      (1L << 14)
#define SSTATUS_FS_SHIFT 13
#define SSTATUS_XS0      (1L << 15)
#define SSTATUS_XS1      (1L << 16)
#define SSTATUS_XS_SHIFT 15
#ifdef PLATFORM_QEMU
#define SSTATUS_SUM       (1L << 18) // User memory accessable, for 1.10+
#define SSTATUS_MXR       (1L << 19)
#define SSTATUS_UXL0      (1L << 32)
#define SSTATUS_UXL1      (1L << 33)
#define SSTATUS_UXL_SHIFT 32
#else
#define SSTATUS_SUM (0L << 18) // User memory accessable, for 1.9 (k210)
#define SSTATUS_PUM (1L << 18) // User memory accessable, for 1.9 (k210)
#endif
#define SSTATUS_SD (1L << 63)

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software

// Supervisor exception program counter, holds the
// instruction address to which a return from
// exception will go.
// sepc

// Machine/Supervisor/User Trap Cause
#define XCAUSE_INT (1L << 63) // Is trap caused by interrupt

// Supervisor Trap Value
// stval

// flush the TLB.
static inline void sfence_vma() { asm volatile("sfence.vma"); }

// Get cpu_cycles
static inline uint64_t cpu_cycle() {
    uint64_t x;
    asm volatile("rdtime %0" : "=r"(x));
    return x;
}
#include <lib/sys/SBI.h>
// Enable/Disable trap(interrupt)
static ALWAYS_INLINE inline void enable_trap() {
    CSR_Write(sstatus, CSR_Read(sstatus) | SSTATUS_SIE);
}

static ALWAYS_INLINE inline void disable_trap() {
    CSR_Write(sstatus, CSR_Read(sstatus) & ~SSTATUS_SIE);
}

static ALWAYS_INLINE inline void     set_cpuid(uint64_t cpuid) { w_tp(cpuid); }
static ALWAYS_INLINE inline uint64_t cpuid() { return r_tp(); }
static ALWAYS_INLINE inline void     flush_tlb_all() { sfence_vma(); }

#endif // __RISCV_H__
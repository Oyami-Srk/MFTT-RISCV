//
// Created by shiroko on 22-5-1.
//

// Barrier code from Linux-RISCV
#ifndef __SMP_BARRIER_H__
#define __SMP_BARRIER_H__

/*
 * __unqual_scalar_typeof(x) - Declare an unqualified scalar type, leaving
 *			       non-scalar types unchanged.
 */
/*
 * Prefer C11 _Generic for better compile-times and simpler code. Note: 'char'
 * is not type-compatible with 'signed char', and we define a separate case.
 */
#define __scalar_type_to_expr_cases(type)                                      \
    unsigned type : (unsigned type)0, signed type : (signed type)0

#define __unqual_scalar_typeof(x)                                              \
    typeof(_Generic((x), char                                                  \
                    : (char)0, __scalar_type_to_expr_cases(char),              \
                      __scalar_type_to_expr_cases(short),                      \
                      __scalar_type_to_expr_cases(int),                        \
                      __scalar_type_to_expr_cases(long),                       \
                      __scalar_type_to_expr_cases(long long), default          \
                    : (x)))

#define READ_ONCE(x) (*(const volatile __unqual_scalar_typeof(x) *)&(x))
#define WRITE_ONCE(x, val)                                                     \
    do {                                                                       \
        *(volatile typeof(x) *)&(x) = (val);                                   \
    } while (0)

#define RISCV_FENCE(p, s) asm volatile("fence " #p "," #s ::: "memory")

#define io_mb()  RISCV_FENCE(iorw, iorw)
#define io_rmb() RISCV_FENCE(ir, ir)
#define io_wmb() RISCV_FENCE(ow, ow)

#define mb()  RISCV_FENCE(rw, rw)
#define rmb() RISCV_FENCE(r, r)
#define wmb() RISCV_FENCE(w, w)

#define compiler_barrier() asm volatile("" ::: "memory")

#endif // __SMP_BARRIER_H__
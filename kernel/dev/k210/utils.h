#ifndef __K210_UTILS_H__
#define __K210_UTILS_H__

#include <riscv.h>
#include <types.h>

static inline void k210_set_bit(volatile uint32_t *bits, uint32_t mask,
                                uint32_t value) {
    uint32_t org = (*bits) & ~mask;
    *bits        = org | (value & mask);
}

static inline void k210_set_bit_offset(volatile uint32_t *bits, uint32_t mask,
                                       uint64_t offset, uint32_t value) {
    k210_set_bit(bits, mask << offset, value << offset);
}

static inline void set_gpio_bit(volatile uint32_t *bits, uint64_t offset,
                                uint32_t value) {
    k210_set_bit_offset(bits, 1, offset, value);
}

static inline uint32_t k210_get_bit(volatile uint32_t *bits, uint32_t mask,
                                    uint64_t offset) {
    return ((*bits) & (mask << offset)) >> offset;
}

static inline uint32_t get_gpio_bit(volatile uint32_t *bits, uint64_t offset) {
    return k210_get_bit(bits, 1, offset);
}

#endif // __K210_UTILS_H__
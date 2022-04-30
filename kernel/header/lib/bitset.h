//
// Created by shiroko on 22-4-24.
//

#ifndef __BITSET_H__
#define __BITSET_H__

#include <common/types.h>

typedef unsigned int bitset;

#define BITS_PER_BITSET (8 * sizeof(bitset))

static inline void set_bit(bitset *bitarray, uint64_t i) {
    bitarray[i / BITS_PER_BITSET] |= (1 << (i % BITS_PER_BITSET));
}

static inline void clear_bit(bitset *bitarray, uint64_t i) {
    bitarray[i / BITS_PER_BITSET] &= ~(1 << (i % BITS_PER_BITSET));
}

static inline int check_bit(bitset *bitarray, uint64_t i) {
    return bitarray[i / BITS_PER_BITSET] & (1 << (i % BITS_PER_BITSET)) ? 1 : 0;
}

static inline void xor_bit(bitset *bitarray, uint64_t i, unsigned int x) {
    bitarray[i / BITS_PER_BITSET] ^= (x << (i % BITS_PER_BITSET));
}

static inline uint64_t set_first_unset_bit(bitset *bitarray, size_t size) {
    for (size_t i = 0; i < size; i++)
        if (bitarray[i] == 0xFFFFFFFF)
            continue;
        else {
            bitset c = ~bitarray[i];

            static const int MultiplyDeBruijnBitPosition[32] = {
                0,  1,  28, 2,  29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4,  8,
                31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6,  11, 5,  10, 9};
            size_t r = MultiplyDeBruijnBitPosition[((bitset)((c & -c) *
                                                             0x077CB531U)) >>
                                                   27];
            bitarray[i] |= (1 << r);
            return r + i * BITS_PER_BITSET;
        }
    return 0xFFFFFFFFFFFFFFFF;
}

#endif // __BITSET_H__
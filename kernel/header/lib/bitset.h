//
// Created by shiroko on 22-4-24.
//

#ifndef __BITSET_H__
#define __BITSET_H__

typedef unsigned int bitset;

#define BITS_PER_BITSET (8 * sizeof(bitset))

static inline void set_bit(bitset *bitarray, unsigned int i) {
    bitarray[i / BITS_PER_BITSET] |= (1 << (i % BITS_PER_BITSET));
}

static inline void clear_bit(bitset *bitarray, unsigned int i) {
    bitarray[i / BITS_PER_BITSET] &= ~(1 << (i % BITS_PER_BITSET));
}

static inline int check_bit(bitset *bitarray, unsigned int i) {
    return bitarray[i / BITS_PER_BITSET] & (1 << (i % BITS_PER_BITSET)) ? 1 : 0;
}

static inline void xor_bit(bitset *bitarray, unsigned int i, unsigned int x) {
    bitarray[i / BITS_PER_BITSET] ^= (1 << (i % BITS_PER_BITSET));
}

#endif // __BITSET_H__
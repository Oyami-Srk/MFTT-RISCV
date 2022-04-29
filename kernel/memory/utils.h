//
// Created by shiroko on 22-4-22.
//

#ifndef __MEMORY_UTILS_H__
#define __MEMORY_UTILS_H__

#include <common/types.h>
#include <memory.h>

static inline uint64_t round_down_power_2(uint64_t x) {
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x |= (x >> 32);
    return x - (x >> 1);
}

static inline uint64_t round_up_power_2(uint64_t x) {
    x--;
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x |= (x >> 32);
    return x + 1;
}

static inline int trailing_zero(uint64_t x) {
    static const unsigned char debruijn_ctz64[64] = {
        63, 0,  1,  52, 2,  6,  53, 26, 3,  37, 40, 7,  33, 54, 47, 27,
        61, 4,  38, 45, 43, 41, 21, 8,  23, 34, 58, 55, 48, 17, 28, 10,
        62, 51, 5,  25, 36, 39, 32, 46, 60, 44, 42, 20, 22, 57, 16, 9,
        50, 24, 35, 31, 59, 19, 56, 15, 49, 30, 18, 14, 29, 13, 12, 11};
    return !x + debruijn_ctz64[((x & -x) * 0x045FBAC7992A70DA) >> 58];
}

static inline void set_page_attr(struct memory_info_t *mem, char *page,
                                 size_t pg_count, int attr) {
    for (size_t i = 0; i < pg_count; i++)
        mem->pages_info[GET_ID_BY_PAGE(*mem, (page + i * PG_SIZE))].type = attr;
}

static inline void clear_page_info(struct memory_info_t *mem, char *page,
                                   size_t pg_count, int attr) {
    for (size_t i = 0; i < pg_count; i++) {
        struct page_info *p =
            &mem->pages_info[GET_ID_BY_PAGE(*mem, (page + i * PG_SIZE))];
        p->type      = attr;
        p->reference = 0;
    }
}

static inline int get_page_reference(struct memory_info_t *mem, char *page) {
    return mem->pages_info[GET_ID_BY_PAGE(*mem, page)].reference;
}

// return an increased value
static inline int increase_page_ref(struct memory_info_t *mem, void *page) {
    return ++(mem->pages_info[GET_ID_BY_PAGE(*mem, page)].reference);
}

// return a decreased value
static inline int decrease_page_ref(struct memory_info_t *mem, void *page) {
    return --(mem->pages_info[GET_ID_BY_PAGE(*mem, page)].reference);
}

#endif // __MEMORY_UTILS_H__
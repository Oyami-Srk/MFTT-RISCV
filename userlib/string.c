//
// Created by shiroko on 22-5-6.
//

#include <string.h>

void *memcpy(void *dst, const void *src, size_t size) {
    const char *s;
    char       *d;
    s = src;
    d = dst;
    if (s < d &&
        s + size > d) { // 当src和dst有重叠时从后向前拷贝，避免覆盖产生的错误
        s += size;
        d += size;
        while (size-- > 0)
            *--d = *--s;
    } else
        while (size-- > 0)
            *d++ = *s++;
    return dst;
}

char *strcpy(char *dst, char *src) {
    char *odst = dst;
    if (src < dst)
        while ((*dst++ = *src++) != '\0')
            ;
    else
        memcpy(dst, src, strlen(src) + 1);
    return odst;
}

size_t strlen(const char *s) {
    const char *eos = s;
    while (*eos++)
        ;
    return (eos - s - 1);
}

void *memset(void *dst, char ch, size_t size) {
    char *cdst = (char *)dst;
    for (size_t i = 0; i < size; i++)
        cdst[i] = ch;
    return dst;
}

int strcmp(const char *cs, const char *ct) {
    unsigned char *c1 = (unsigned char *)cs;
    unsigned char *c2 = (unsigned char *)ct;
    while (1) {
        if (*c1 != *c2)
            return *c1 < *c2 ? -1 : 1;
        if (!*c1)
            break;
        c1++;
        c2++;
    }
    return 0;
}

int memcmp(const char *cs, const char *ct, size_t count) {
    unsigned char *c1 = (unsigned char *)cs;
    unsigned char *c2 = (unsigned char *)ct;
    while (count-- > 0) {
        if (*c1 != *c2)
            return *c1 < *c2 ? -1 : 1;
        c1++;
        c2++;
    }
    return 0;
}

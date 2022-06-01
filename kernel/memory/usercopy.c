//
// Created by shiroko on 22-5-20.
//

#include <lib/stdlib.h>
#include <lib/string.h>
#include <memory.h>
#include <riscv.h>
#include <types.h>

#define KERNEL_MEM_START 0x80000000

static ALWAYS_INLINE inline void *umem_access_memcpy(void *dst, const void *src,
                                                     size_t size) {
    BEGIN_UMEM_ACCESS();
    void *r = memcpy(dst, src, size);
    STOP_UMEM_ACCESS();
    return r;
}

static void *memcpy_k2u(void *dst, const void *src, size_t size) {
    umem_access_memcpy(dst, src, size);
}

static void *memcpy_u2k(void *dst, const void *src, size_t size) {
    umem_access_memcpy(dst, src, size);
}

static void *memcpy_u2u(void *dst, const void *src, size_t size) {
    umem_access_memcpy(dst, src, size);
}

void *umemcpy(void *dst, const void *src, size_t size) {
    if ((uintptr_t)dst < KERNEL_MEM_START) {
        if ((uintptr_t)src >= KERNEL_MEM_START) {
            assert((uintptr_t)dst + size < KERNEL_MEM_START,
                   "umemcpy execeed.");
            return memcpy_k2u(dst, src, size);
        } else {
            assert((uintptr_t)dst + size < KERNEL_MEM_START,
                   "umemcpy execeed.");
            assert((uintptr_t)src + size < KERNEL_MEM_START,
                   "umemcpy execeed.");
            return memcpy_u2u(dst, src, size);
        }
    }
    if ((uintptr_t)src < KERNEL_MEM_START) {
        assert((uintptr_t)src + size < KERNEL_MEM_START, "umemcpy execeed.");
        return memcpy_u2k(dst, src, size);
    }
    kpanic("umemory on all kernel memory.");
    return NULL;
}

void *umemset(void *dst, char c, size_t size) {
    BEGIN_UMEM_ACCESS();
    assert((uintptr_t)dst < KERNEL_MEM_START, "umemset set kernel memory.");
    memset(dst, 0, size);
    STOP_UMEM_ACCESS();
}

char *ustrcpy_out(char *ustr) {
    BEGIN_UMEM_ACCESS();
    size_t len = strlen(ustr);
    assert((uintptr_t)ustr + len + 1 < KERNEL_MEM_START, "user str execeed.");
    char *kbuf = (char *)kmalloc(len + 1);
    if (!kbuf)
        return NULL;
    memcpy(kbuf, ustr, len);
    kbuf[len] = '\0';
    STOP_UMEM_ACCESS();
    return kbuf;
}

void ustrcpy_in(char *ustr, char *kbuf) {
    size_t len = strlen(kbuf);
    assert((uintptr_t)ustr + len + 1 < KERNEL_MEM_START,
           "user str buffer too small.");
    memcpy_k2u(ustr, kbuf, len + 1);
    kfree(kbuf);
}

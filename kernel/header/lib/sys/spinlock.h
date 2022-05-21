#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <types.h>

typedef struct __spinlock_t {
    bool lock;
    int  cpu;
} spinlock_t;

void spinlock_init(spinlock_t *pLock);
void spinlock_acquire(spinlock_t *pLock);
void spinlock_release(spinlock_t *pLock);

#endif // __SPINLOCK_H__
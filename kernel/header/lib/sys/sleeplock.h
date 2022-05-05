//
// Created by shiroko on 22-5-4.
//

#ifndef __SLEEPLOCK_H__
#define __SLEEPLOCK_H__

#include <lib/sys/spinlock.h>
#include <proc.h>

typedef struct {
    bool       lock;
    spinlock_t spinlock;

    pid_t pid;
} sleeplock_t;

void sleeplock_init(sleeplock_t *pLock);
void sleeplock_acquire(sleeplock_t *pLock);
void sleeplock_release(sleeplock_t *pLock);

#endif // __SLEEPLOCK_H__
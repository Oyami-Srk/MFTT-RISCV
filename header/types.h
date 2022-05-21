//
// Created by shiroko on 22-4-18.
//

#ifndef __TYPES_H__
#define __TYPES_H__

typedef _Bool bool;
#define TRUE  1
#define FALSE 0
#define true  1
#define false 0

#define NULL ((void *)(0))

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef uint64_t size_t;
typedef uint64_t uintptr_t;

typedef __builtin_va_list va_list;
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap)          __builtin_va_end(ap)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)

#endif // __TYPES_H__
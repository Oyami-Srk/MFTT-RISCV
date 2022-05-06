#ifndef __STDLIB_H__
#define __STDLIB_H__

#include <types.h>

char *itoa(long long value, char *str, int base);
int   vsprintf(char *buf, const char *fmt, va_list args);
int   sprintf(char *buf, const char *fmt, ...);

#endif // __STDLIB_H__
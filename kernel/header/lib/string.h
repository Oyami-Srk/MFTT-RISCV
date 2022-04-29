#ifndef __STRING_H__
#define __STRING_H__

char  *strcpy(char *dst, char *src);
size_t strlen(const char *s);
void  *memcpy(void *dst, const void *src, size_t size);
void  *memset(void *dst, char ch, size_t size);
int    strcmp(const char *cs, const char *ct);
int    memcmp(const char *cs, const char *ct, size_t count);

#endif // __STRING_H__
#ifndef __STDLIB_H__
#define __STDLIB_H__

#include <common/types.h>

char *itoa(long long value, char *str, int base);
int   vsprintf(char *buf, const char *fmt, va_list args);
int   sprintf(char *buf, const char *fmt, ...);

extern _Noreturn void kpanic_proto(const char *s_fn, const char *b_fn,
                                   const int line, const char *fmt, ...);
#define kpanic(str, ...)                                                       \
    kpanic_proto(__FILE__, __BASE_FILE__, __LINE__, str, ##__VA_ARGS__)
#define assert(exp, message)                                                   \
    if (unlikely(!(exp)))                                                      \
    kpanic(message "(Assertion \"" #exp "\" failed.)")

#define section_foreach_entry(section_name, type_t, elem)                      \
    for (type_t *elem = ({                                                     \
             extern type_t __start_##section_name;                             \
             &__start_##section_name;                                          \
         });                                                                   \
         elem != ({                                                            \
             extern type_t __stop_##section_name;                              \
             &__stop_##section_name;                                           \
         });                                                                   \
         elem++)

#define offset_of(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member)                                        \
    ({                                                                         \
        const typeof(((type *)0)->member) *__mptr = (ptr);                     \
        (type *)((char *)__mptr - offset_of(type, member));                    \
    })

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#endif // __STDLIB_H__
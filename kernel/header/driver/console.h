#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#define MAX_CONSOLE_BUF 1024

void           init_console();
void           kprintf(const char *fmt, ...);
void           kputc(const char c);
_Noreturn void kpanic_proto(const char *s_fn, const char *b_fn, const int line,
                            const char *fmt, ...);

#endif // __CONSOLE_H__
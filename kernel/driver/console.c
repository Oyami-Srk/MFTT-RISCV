#include <driver/SBI.h>
#include <driver/console.h>
#include <lib/stdlib.h>
#include <lib/sys/spinlock.h>

struct {
    spinlock_t lock;
} console;

void init_console() { spinlock_init(&console.lock); }

void kprintf(const char *fmt, ...) {
    spinlock_acquire(&console.lock);
    int         i;
    static char buf[MAX_CONSOLE_BUF];
    va_list     arg;
    va_start(arg, fmt);
    i       = vsprintf(buf, fmt, arg);
    buf[i]  = 0;
    char *s = buf;
    while (*s != '\0')
        SBI_putchar(*(s++));
    spinlock_release(&console.lock);
}

void kputc(const char c) { SBI_putchar(c); }

static void print_str(const char *buf) {
    char *s = (char *)buf;
    while (*s != '\0')
        SBI_putchar(*(s++));
}

_Noreturn void kpanic_proto(const char *s_fn, const char *b_fn, const int line,
                            const char *fmt, ...) {
    print_str("\n\n==================================================\n");
    print_str("[PANIC] ");
    int     i;
    char    buf[MAX_CONSOLE_BUF];
    va_list arg = (va_list)((char *)(&fmt) + 4);
    i           = vsprintf(buf, fmt, arg);
    buf[i]      = 0;
    char *s     = buf;
    while (*s != '\0')
        SBI_putchar(*(s++));
    print_str("\nAt file: ");
    print_str(s_fn);
    if (s_fn != b_fn) {
        print_str("; Base file: ");
        print_str(b_fn);
    }
    print_str("; Line: ");
    char num_buf[32] = "\0";
    itoa(line, num_buf, 10);
    print_str(num_buf);
    print_str("\n==================================================\n\n");
    // Panic halt
    //    SBI_shutdown();
    while (1)
        ;
}

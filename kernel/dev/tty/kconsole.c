// FIXME: kconsole depends on SBI.

#include <driver/console.h>
#include <lib/stdlib.h>
#include <lib/sys/SBI.h>
#include <lib/sys/spinlock.h>

static spinlock_t kprintf_lock = {.lock = false, .cpu = 0};

void init_kprintf() { spinlock_init(&kprintf_lock); }

void kprintf(const char *fmt, ...) {
    spinlock_acquire(&kprintf_lock);
    int         i;
    static char buf[MAX_CONSOLE_BUF];
    va_list     arg;
    va_start(arg, fmt);
    i       = vsprintf(buf, fmt, arg);
    buf[i]  = 0;
    char *s = buf;
    while (*s != '\0')
        SBI_putchar(*(s++));
    spinlock_release(&kprintf_lock);
}

void kputc(const char c) { SBI_putchar(c); }

spinlock_t kpanic_lock = {.lock = false, .cpu = 0};

static void print_str_nolock(const char *buf) {
    char *s = (char *)buf;
    while (*s != '\0')
        SBI_putchar(*(s++));
}

_Noreturn void kpanic_proto(const char *s_fn, const char *b_fn, const int line,
                            const char *fmt, ...) {
    spinlock_acquire(&kpanic_lock);
    print_str_nolock(
        "\n\n==================================================\n");
    print_str_nolock("[PANIC] ");
    int         i;
    static char buf[MAX_CONSOLE_BUF];
    va_list     arg = (va_list)((char *)(&fmt) + 4);
    i               = vsprintf(buf, fmt, arg);
    buf[i]          = 0;
    char *s         = buf;
    while (*s != '\0')
        SBI_putchar(*(s++));
    print_str_nolock("\nAt file: ");
    print_str_nolock(s_fn);
    if (s_fn != b_fn) {
        print_str_nolock("; Base file: ");
        print_str_nolock(b_fn);
    }
    print_str_nolock("; Line: ");
    char num_buf[32] = "\0";
    itoa(line, num_buf, 10);
    print_str_nolock(num_buf);
    print_str_nolock(
        "\n==================================================\n\n");
    // Panic halt
    //    SBI_shutdown();
    spinlock_release(&kpanic_lock);
    while (1)
        ;
}

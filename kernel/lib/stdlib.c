#include <lib/stdlib.h>
#include <lib/string.h>

char *itoa(long long value, char *str, int base) {
    char *rc;
    char *ptr;
    char *low;
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    rc = ptr = str;
    if (value < 0 && base == 10)
        *ptr++ = '-';
    low = ptr;
    do {
        *ptr++ =
            "ZYXWVUTSRQPONMLKJIHGFEDCBA9876543210123456789ABCDEFGHIJKLMNOPQRST"
            "UVWXYZ"[35 + value % base];
        value /= base;
    } while (value);
    *ptr-- = '\0';
    while (low < ptr) {
        char tmp = *low;
        *low++   = *ptr;
        *ptr--   = tmp;
    }
    return rc;
}

int vsprintf(char *buf, const char *fmt, va_list args) {
    va_list   arg = args;
    long long m;

    static char inner_buf[1024]; // hardcode is not good
    char        cs;
    int         align;
    int         align_type = 0; // 0 - right, 1 - left, 2 - center
    int         longint    = 0; // 0 - int, 1 - long int

    char *p;
    for (p = buf; *fmt; fmt++) {
        if (*fmt != '%') {
            *p++ = *fmt;
            continue;
        } else {
            align      = 0;
            align_type = 0;
            longint    = 0;
        }
        fmt++;
        if (*fmt == '%') {
            *p++ = *fmt;
            continue;
        } else if (*fmt == '-') {
            align_type = 1;
            fmt++;
        } else if (*fmt == '.') {
            align_type = 2;
            fmt++;
        }

        if (*fmt == '0') {
            cs = '0';
            fmt++;
        } else {
            cs = ' ';
        }

        while (((unsigned char)(*fmt) >= '0') &&
               ((unsigned char)(*fmt) <= '9')) {
            align *= 10;
            align += *fmt - '0';
            fmt++;
        }

        char *q = inner_buf;
        memset((void *)q, 0, sizeof(inner_buf));

        if (*fmt == 'l') {
            longint = 1;
            fmt++;
        }

        switch (*fmt) {
        case 'c':
            *q++ = *((char *)arg);
            arg += sizeof(char *);
            break;
        case 'x':
            if (longint)
                m = *((long long *)arg);
            else
                m = *((int *)arg);
            itoa(m, q, 16);
            arg += sizeof(int *); // pointer size are the same
            break;
        case 'p':
            if (longint)
                m = *((long long *)arg);
            else
                m = *((int *)arg);
            *(q++) = '0';
            *(q++) = 'x';
            itoa(m, q, 16);
            arg += sizeof(int *); // pointer size are the same
            break;
        case 'd':
            if (longint)
                m = *((long long *)arg);
            else
                m = *((int *)arg);
            if (m < 0) {
                m    = m * (-1);
                *q++ = '-';
            }
            itoa(m, q, 10);
            arg += sizeof(int *);
            break;
        case 's':
            strcpy(q, (*((char **)arg)));
            q += strlen(*((char **)arg));
            arg += sizeof(char **);
            break;
        default:
            break;
        }
        int len = strlen(inner_buf);
        if (align > len) {
            switch (align_type) {
            case 0: // right
                for (int k = 0; k < align - len; k++)
                    *p++ = cs;
                break;
            case 2: // middle
                for (int k = 0; k < (align - len) / 2; k++)
                    *p++ = cs;
                break;
            default:
                break;
            }
        }
        /*
        for (int k = 0;
             k <
             ((align > strlen(inner_buf)) ? (align - strlen(inner_buf)) : 0);
             k++)
            *p++ = cs;*/
        q = inner_buf;
        while (*q)
            *p++ = *q++;
        if (align_type > 0 && align > len) {
            switch (align_type) {
            case 1: // left
                for (int k = 0; k < align - len; k++)
                    *p++ = cs;
                break;
            case 2: // middle
                for (int k = 0; k < (align - len) / 2 + ((align - len) % 2);
                     k++)
                    *p++ = cs;
                break;
            default:
                break;
            }
        }
    }

    *p = 0;
    return (p - buf);
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list arg = (va_list)((char *)(&fmt) + 4);
    return vsprintf(buf, fmt, arg);
}
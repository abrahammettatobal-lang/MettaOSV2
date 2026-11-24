#include "log/klog.h"
#include "log/printk.h"

#include <stdarg.h>
#include <stdint.h>

#include "serial.h"

static klog_level_t g_klog_level = KLOG_DEFAULT_LEVEL;

static const char *klog_level_tag(klog_level_t level)
{
    switch (level) {
    case KLOG_LEVEL_ERR:
        return "[E] ";
    case KLOG_LEVEL_WARN:
        return "[W] ";
    case KLOG_LEVEL_INFO:
        return "[I] ";
    case KLOG_LEVEL_DEBUG:
        return "[D] ";
    default:
        return "[?] ";
    }
}

klog_level_t klog_get_level(void)
{
    return g_klog_level;
}

void klog_set_level(klog_level_t level)
{
    if (level > KLOG_LEVEL_DEBUG) {
        level = KLOG_LEVEL_DEBUG;
    }
    g_klog_level = level;
}

static void printk_put_u32(unsigned int value, unsigned int base, int uppercase)
{
    char buf[32];
    int i = 0;

    if (value == 0u) {
        serial_putc('0');
        return;
    }

    while (value > 0u && i < (int)sizeof(buf)) {
        unsigned int digit = value % base;
        value /= base;
        if (digit < 10u) {
            buf[i++] = (char)('0' + digit);
        } else {
            char letter = uppercase ? 'A' : 'a';
            buf[i++] = (char)(letter + (int)(digit - 10u));
        }
    }

    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

static void printk_put_u64(unsigned long long value, unsigned int base, int uppercase)
{
    char buf[32];
    int i = 0;

    if (value == 0ull) {
        serial_putc('0');
        return;
    }

    while (value > 0ull && i < (int)sizeof(buf)) {
        unsigned long long digit = value % base;
        value /= base;
        if (digit < 10ull) {
            buf[i++] = (char)('0' + digit);
        } else {
            char letter = uppercase ? 'A' : 'a';
            buf[i++] = (char)(letter + (int)(digit - 10ull));
        }
    }

    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

static void printk_put_i64(long long value, unsigned int base)
{
    if (value < 0) {
        serial_putc('-');
        value = -value;
    }
    printk_put_u64((unsigned long long)value, base, 0);
}

void vprintk(const char *fmt, va_list args)
{
    if (fmt == NULL) {
        return;
    }

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            serial_putc(*p);
            continue;
        }

        p++;
        switch (*p) {
        case '%':
            serial_putc('%');
            break;
        case 'c': {
            int c = va_arg(args, int);
            serial_putc((char)c);
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            serial_write_str(s != NULL ? s : "(null)");
            break;
        }
        case 'd': {
            int v = va_arg(args, int);
            if (v < 0) {
                serial_putc('-');
                v = -v;
            }
            printk_put_u32((unsigned int)v, 10, 0);
            break;
        }
        case 'u':
            printk_put_u32(va_arg(args, unsigned int), 10, 0);
            break;
        case 'x':
            printk_put_u32(va_arg(args, unsigned int), 16, 0);
            break;
        case 'X':
            printk_put_u32(va_arg(args, unsigned int), 16, 1);
            break;
        case 'p': {
            void *ptr = va_arg(args, void *);
            serial_write_str("0x");
            printk_put_u64((unsigned long long)(uintptr_t)ptr, 16, 0);
            break;
        }
        case 'l':
            if (*(p + 1) == 'l') {
                if (*(p + 2) == 'u') {
                    p += 2;
                    printk_put_u64(va_arg(args, unsigned long long), 10, 0);
                } else if (*(p + 2) == 'd') {
                    p += 2;
                    printk_put_i64(va_arg(args, long long), 10);
                } else if (*(p + 2) == 'x') {
                    p += 2;
                    printk_put_u64(va_arg(args, unsigned long long), 16, 0);
                } else {
                    serial_putc('%');
                    serial_putc('l');
                    serial_putc('l');
                }
            } else if (*(p + 1) == 'u') {
                p++;
                printk_put_u64(va_arg(args, unsigned long), 10, 0);
            } else if (*(p + 1) == 'x') {
                p++;
                printk_put_u64(va_arg(args, unsigned long), 16, 0);
            } else if (*(p + 1) == 'd') {
                p++;
                printk_put_i64(va_arg(args, long), 10);
            } else {
                serial_putc('%');
                serial_putc('l');
            }
            break;
        default:
            serial_putc('%');
            if (*p != '\0') {
                serial_putc(*p);
            }
            break;
        }
    }
}

void printk(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

void klog_emit(klog_level_t level, const char *fmt, ...)
{
    va_list args;

    if (level > g_klog_level) {
        return;
    }

    serial_write_str(klog_level_tag(level));
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}
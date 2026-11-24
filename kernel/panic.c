#include "log/panic.h"
#include "log/printk.h"

#include <stdarg.h>

#include "serial.h"

void panic_halt(void)
{
    __asm__ volatile("cli");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void panic(const char *msg)
{
    serial_write_str("\r\n*** METTAOS PANIC ***\r\n");
    if (msg == NULL) {
        msg = "(null)";
    }
    printk("panic: %s\n", msg);
    panic_halt();
}

void panic_fmt(const char *fmt, ...)
{
    va_list args;

    serial_write_str("\r\n*** METTAOS PANIC ***\r\n");
    serial_write_str("panic: ");
    if (fmt == NULL) {
        serial_write_str("(null)\n");
        panic_halt();
    }
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
    serial_putc('\n');
    panic_halt();
}
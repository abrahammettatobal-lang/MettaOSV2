#ifndef METTA_SERIAL_H
#define METTA_SERIAL_H

#include <stddef.h>
#include <stdint.h>

/* Puerto COM1 (base 0x3F8). kmain debe llamar serial_init() antes de printk/PIT diag. */

#define SERIAL_COM1_BASE 0x3F8u

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *buf, size_t len);
void serial_write_str(const char *s);

#endif /* METTA_SERIAL_H */
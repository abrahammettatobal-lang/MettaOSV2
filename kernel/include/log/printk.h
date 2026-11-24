#ifndef METTA_LOG_PRINTK_H
#define METTA_LOG_PRINTK_H

#include <stdarg.h>

void printk(const char *fmt, ...);
void vprintk(const char *fmt, va_list args);

#endif /* METTA_LOG_PRINTK_H */
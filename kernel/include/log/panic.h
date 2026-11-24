#ifndef METTA_LOG_PANIC_H
#define METTA_LOG_PANIC_H

/*
 * Punto de no-retorno del kernel. Emite diagnóstico por COM1 y detiene la CPU.
 * Requiere serial_init() antes del primer panic (SA-06).
 */

__attribute__((noreturn)) void panic(const char *msg);
__attribute__((noreturn)) void panic_fmt(const char *fmt, ...);

void panic_halt(void) __attribute__((noreturn));

#endif /* METTA_LOG_PANIC_H */
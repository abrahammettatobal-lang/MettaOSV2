#ifndef METTA_TTY_H
#define METTA_TTY_H

#include <stddef.h>
#include <stdint.h>

/*
 * Capa TTY del kernel MettaOS (SA-28).
 * Prepara disciplina de línea y anillos para sesiones de consola.
 * No es POSIX completo: pty, job control y señales llegan con SA-10/SA-32.
 */

#define TTY_INPUT_RING_SIZE   2048u
#define TTY_OUTPUT_RING_SIZE  8192u
#define TTY_MAX_DEVICES       4u

#define TTY_VT_DEFAULT        0u

typedef uint32_t tty_id_t;
typedef struct tty_device tty_device_t;

void tty_init(void);

tty_device_t *tty_get(tty_id_t id);
tty_device_t *tty_default(void);

/* Escribe bytes en la salida del TTY (eco hacia terminal gráfica / lectores). */
size_t tty_write(tty_device_t *tty, const char *buf, size_t len);

/*
 * Lee bytes disponibles del anillo de entrada (p. ej. teclado vía SA-13).
 * Retorna 0 si no hay datos.
 */
size_t tty_read(tty_device_t *tty, char *buf, size_t max_len);

/* Inyecta entrada desde el driver de teclado o la app gráfica. */
void tty_input_byte(tty_device_t *tty, uint8_t byte);

/* Drenar salida hacia la terminal gráfica; retorna bytes copiados. */
size_t tty_drain_output(tty_device_t *tty, char *buf, size_t max_len);

/* Si no es NULL, eco de diagnóstico hacia UART (serial). */
void tty_set_serial_mirror(int enable);

#endif /* METTA_TTY_H */
#ifndef MDE_TERMINAL_H
#define MDE_TERMINAL_H

#include <stddef.h>
#include <stdint.h>

/*
 * Terminal gráfica MDE (SA-28).
 * Renderiza una cuadrícula de celdas conectada a un tty del kernel.
 */

#define MDE_TERM_DEFAULT_COLS 80u
#define MDE_TERM_DEFAULT_ROWS 24u

typedef struct tty_device tty_device_t;

typedef struct mde_term_cell {
    char     ch;
    uint32_t fg;
    uint32_t bg;
} mde_term_cell_t;

typedef struct mde_terminal {
    uint16_t        cols;
    uint16_t        rows;
    mde_term_cell_t *cells;
    uint16_t        cursor_col;
    uint16_t        cursor_row;
    tty_device_t   *tty;
    uint32_t        default_fg;
    uint32_t        default_bg;
} mde_terminal_t;

int  mde_terminal_create(mde_terminal_t *term, uint16_t cols, uint16_t rows);
void mde_terminal_destroy(mde_terminal_t *term);

void mde_terminal_attach_tty(mde_terminal_t *term, tty_device_t *tty);

/* Aplica bytes del TTY a la cuadrícula (secuencias mínimas: \\n, \\r, \\b). */
void mde_terminal_feed(mde_terminal_t *term, const char *data, size_t len);

/* Sincroniza salida pendiente del TTY con la vista. */
void mde_terminal_sync_from_tty(mde_terminal_t *term);

/* Tecla desde input MDE → TTY. */
void mde_terminal_key_char(mde_terminal_t *term, char c);

void mde_terminal_clear(mde_terminal_t *term);

const mde_term_cell_t *mde_terminal_cell_at(const mde_terminal_t *term,
                                            uint16_t col, uint16_t row);

#endif /* MDE_TERMINAL_H */
/*
 * MettaOS MDE — terminal gráfica (SA-28).
 */
#include "terminal.h"

#include <stddef.h>

#include "tty/tty.h"

/* Valores ARGB opacos MDE Light (coinciden con mde/theme/light.h). */
#define MDE_TERM_FG 0xFFE8ECF0u
#define MDE_TERM_BG 0xFF1E2228u
#define MDE_TERM_CURSOR_BG 0xFF3278D9u

#define MDE_TERM_CELL_CAP (120u * 40u)
static mde_term_cell_t g_term_storage[MDE_TERM_CELL_CAP];
static int             g_term_storage_bound;

static mde_term_cell_t *term_cell(mde_terminal_t *term, uint16_t col, uint16_t row)
{
    uint32_t idx;

    if (term == NULL || term->cells == NULL || col >= term->cols || row >= term->rows) {
        return NULL;
    }
    idx = (uint32_t)row * (uint32_t)term->cols + (uint32_t)col;
    return &term->cells[idx];
}

static void term_scroll(mde_terminal_t *term)
{
    uint16_t r;
    uint16_t c;

    if (term == NULL || term->cells == NULL || term->rows <= 1u) {
        return;
    }

    for (r = 1u; r < term->rows; r++) {
        for (c = 0; c < term->cols; c++) {
            mde_term_cell_t *dst = term_cell(term, c, (uint16_t)(r - 1u));
            mde_term_cell_t *src = term_cell(term, c, r);
            if (dst != NULL && src != NULL) {
                *dst = *src;
            }
        }
    }

    for (c = 0; c < term->cols; c++) {
        mde_term_cell_t *last = term_cell(term, c, (uint16_t)(term->rows - 1u));
        if (last != NULL) {
            last->ch = ' ';
            last->fg = term->default_fg;
            last->bg = term->default_bg;
        }
    }

    if (term->cursor_row > 0u) {
        term->cursor_row--;
    }
}

static void term_putc(mde_terminal_t *term, char ch)
{
    mde_term_cell_t *cell;

    if (term == NULL) {
        return;
    }

    if (ch == '\n') {
        term->cursor_col = 0;
        if (term->cursor_row + 1u >= term->rows) {
            term_scroll(term);
        } else {
            term->cursor_row++;
        }
        return;
    }

    if (ch == '\r') {
        term->cursor_col = 0;
        return;
    }

    if (ch == '\b') {
        if (term->cursor_col > 0u) {
            term->cursor_col--;
        }
        return;
    }

    if (ch == '\t') {
        term->cursor_col = (uint16_t)((term->cursor_col + 8u) & ~7u);
        if (term->cursor_col >= term->cols) {
            term_putc(term, '\n');
        }
        return;
    }

    cell = term_cell(term, term->cursor_col, term->cursor_row);
    if (cell != NULL) {
        cell->ch = ch;
        cell->fg = term->default_fg;
        cell->bg = term->default_bg;
    }

    term->cursor_col++;
    if (term->cursor_col >= term->cols) {
        term_putc(term, '\n');
    }
}

int mde_terminal_create(mde_terminal_t *term, uint16_t cols, uint16_t rows)
{
    uint32_t count;
    uint32_t i;

    if (term == NULL || cols == 0u || rows == 0u) {
        return -1;
    }

    count = (uint32_t)cols * (uint32_t)rows;
    if (count > MDE_TERM_CELL_CAP) {
        return -2;
    }

    term->cols = cols;
    term->rows = rows;
    term->cells = g_term_storage;
    term->cursor_col = 0;
    term->cursor_row = 0;
    term->tty = NULL;
    term->default_fg = MDE_TERM_FG;
    term->default_bg = MDE_TERM_BG;

    for (i = 0; i < count; i++) {
        term->cells[i].ch = ' ';
        term->cells[i].fg = term->default_fg;
        term->cells[i].bg = term->default_bg;
    }

    g_term_storage_bound = (int)count;
    return 0;
}

void mde_terminal_destroy(mde_terminal_t *term)
{
    if (term == NULL) {
        return;
    }
    term->cells = NULL;
    term->tty = NULL;
}

void mde_terminal_attach_tty(mde_terminal_t *term, tty_device_t *tty)
{
    if (term == NULL) {
        return;
    }
    term->tty = tty;
}

void mde_terminal_feed(mde_terminal_t *term, const char *data, size_t len)
{
    size_t i;

    if (term == NULL || data == NULL) {
        return;
    }

    for (i = 0; i < len; i++) {
        term_putc(term, data[i]);
    }
}

void mde_terminal_sync_from_tty(mde_terminal_t *term)
{
    char chunk[256];
    size_t n;

    if (term == NULL || term->tty == NULL) {
        return;
    }

    for (;;) {
        n = tty_drain_output(term->tty, chunk, sizeof(chunk));
        if (n == 0u) {
            break;
        }
        mde_terminal_feed(term, chunk, n);
    }
}

void mde_terminal_key_char(mde_terminal_t *term, char c)
{
    if (term == NULL || term->tty == NULL) {
        return;
    }
    tty_input_byte(term->tty, (uint8_t)c);
    mde_terminal_sync_from_tty(term);
}

void mde_terminal_clear(mde_terminal_t *term)
{
    uint32_t count;
    uint32_t i;

    if (term == NULL || term->cells == NULL) {
        return;
    }

    count = (uint32_t)term->cols * (uint32_t)term->rows;
    for (i = 0; i < count && i < (uint32_t)g_term_storage_bound; i++) {
        term->cells[i].ch = ' ';
        term->cells[i].fg = term->default_fg;
        term->cells[i].bg = term->default_bg;
    }

    term->cursor_col = 0;
    term->cursor_row = 0;
}

const mde_term_cell_t *mde_terminal_cell_at(const mde_terminal_t *term,
                                            uint16_t col, uint16_t row)
{
    mde_term_cell_t *cell;

    if (term == NULL) {
        return NULL;
    }
    cell = term_cell((mde_terminal_t *)term, col, row);
    return cell;
}
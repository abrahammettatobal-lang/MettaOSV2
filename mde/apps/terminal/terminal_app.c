/*
 * Punto de ensamble: ventana Terminal MDE + tty0 + Metta Shell (msh).
 */
#include "msh.h"
#include "terminal.h"

#include "tty/tty.h"

typedef struct mde_terminal_app {
    mde_terminal_t term;
    msh_session_t  shell;
} mde_terminal_app_t;

static mde_terminal_app_t g_term_app;

int mde_terminal_app_start(void)
{
    tty_device_t *tty;

    tty_init();
    tty = tty_default();
    if (tty == NULL) {
        return -1;
    }

    tty_set_serial_mirror(0);

    if (mde_terminal_create(&g_term_app.term, MDE_TERM_DEFAULT_COLS, MDE_TERM_DEFAULT_ROWS) != 0) {
        return -2;
    }

    mde_terminal_attach_tty(&g_term_app.term, tty);
    msh_session_init(&g_term_app.shell, tty);
    mde_terminal_sync_from_tty(&g_term_app.term);

    return 0;
}

void mde_terminal_app_key(mde_terminal_app_t *app, char c)
{
    if (app == NULL) {
        app = &g_term_app;
    }

    msh_on_input_char(&app->shell, c);
    if (tty_default() != NULL) {
        mde_terminal_sync_from_tty(&app->term);
    }
}

mde_terminal_t *mde_terminal_app_view(void)
{
    return &g_term_app.term;
}
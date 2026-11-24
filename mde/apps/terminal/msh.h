#ifndef MDE_MSH_H
#define MDE_MSH_H

#include <stddef.h>

#include "tty/tty.h"

/*
 * Metta Shell (msh) — shell inicial nativo de MettaOS.
 * No es GNU Bash; Bash es software GNU ajeno (SA-32), no un «shell casero».
 */

#define MSH_NAME        "msh"
#define MSH_VERSION     "0.1.0-sa28"
#define MSH_PROMPT      "metta$ "

typedef struct msh_session {
    tty_device_t *tty;
    char          line_buf[256];
    size_t        line_len;
    int           running;
} msh_session_t;

void msh_session_init(msh_session_t *sess, tty_device_t *tty);
void msh_session_stop(msh_session_t *sess);

/* Una línea terminada en \\n desde el TTY. */
void msh_on_line_submit(msh_session_t *sess, const char *line);

/* Carácter de teclado hacia el intérprete de línea del shell. */
void msh_on_input_char(msh_session_t *sess, char c);

#endif /* MDE_MSH_H */
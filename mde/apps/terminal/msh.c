/*
 * MettaOS — Metta Shell (msh), shell inicial del sistema.
 * GNU Bash (cuando exista compatibilidad Linux) se documenta como upstream GNU,
 * nunca como «shell casero» de MettaOS.
 */
#include "msh.h"

#include <stddef.h>

#define MSH_BUILD_ID "MettaOS SA-28"

static int msh_str_eq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static const char *msh_skip_ws(const char *s)
{
    if (s == NULL) {
        return "";
    }
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

static void msh_write_line(tty_device_t *tty, const char *text)
{
    if (tty == NULL || text == NULL) {
        return;
    }
    (void)tty_write(tty, text, __builtin_strlen(text));
    (void)tty_write(tty, "\n", 1u);
}

static void msh_write_prompt(tty_device_t *tty)
{
    if (tty == NULL) {
        return;
    }
    (void)tty_write(tty, MSH_PROMPT, sizeof(MSH_PROMPT) - 1u);
}

static void msh_cmd_help(tty_device_t *tty)
{
    msh_write_line(tty, "Metta Shell (msh) — shell inicial nativo.");
    msh_write_line(tty, "Comandos: help, echo, clear, uname, exit");
    msh_write_line(tty, "Nota: GNU Bash no forma parte de msh; es un intérprete GNU aparte.");
}

static void msh_cmd_echo(tty_device_t *tty, const char *args)
{
    args = msh_skip_ws(args);
    if (*args == '\0') {
        msh_write_line(tty, "");
        return;
    }
    (void)tty_write(tty, args, __builtin_strlen(args));
    (void)tty_write(tty, "\n", 1u);
}

static void msh_cmd_uname(tty_device_t *tty)
{
    (void)tty_write(tty, MSH_BUILD_ID, sizeof(MSH_BUILD_ID) - 1u);
    (void)tty_write(tty, " ", 1u);
    (void)tty_write(tty, MSH_VERSION, sizeof(MSH_VERSION) - 1u);
    (void)tty_write(tty, "\n", 1u);
}

void msh_session_init(msh_session_t *sess, tty_device_t *tty)
{
    if (sess == NULL) {
        return;
    }

    sess->tty = tty;
    sess->line_len = 0;
    sess->running = 1;
    sess->line_buf[0] = '\0';

    if (tty != NULL) {
        msh_write_line(tty, "Bienvenido a Metta Shell (msh). Escriba 'help'.");
        msh_write_prompt(tty);
    }
}

void msh_session_stop(msh_session_t *sess)
{
    if (sess == NULL) {
        return;
    }
    sess->running = 0;
}

void msh_on_line_submit(msh_session_t *sess, const char *line)
{
    const char *cmd;
    const char *rest;
    tty_device_t *tty;

    if (sess == NULL || line == NULL) {
        return;
    }

    tty = sess->tty;
    line = msh_skip_ws(line);
    if (*line == '\0') {
        msh_write_prompt(tty);
        return;
    }

    cmd = line;
    rest = line;
    while (*rest != '\0' && *rest != ' ' && *rest != '\t') {
        rest++;
    }

    if (msh_str_eq(cmd, "help")) {
        msh_cmd_help(tty);
    } else if (msh_str_eq(cmd, "echo")) {
        while (*rest == ' ' || *rest == '\t') {
            rest++;
        }
        msh_cmd_echo(tty, rest);
    } else if (msh_str_eq(cmd, "clear")) {
        (void)tty_write(tty, "\x1b[2J", 4u); /* reservado para VT futuro */
        msh_write_line(tty, "(clear — vista gráfica vía mde_terminal_clear)");
    } else if (msh_str_eq(cmd, "uname")) {
        msh_cmd_uname(tty);
    } else if (msh_str_eq(cmd, "exit")) {
        msh_write_line(tty, "Sesión msh finalizada.");
        sess->running = 0;
        return;
    } else {
        msh_write_line(tty, "msh: comando desconocido (no es Bash).");
    }

    if (sess->running) {
        msh_write_prompt(tty);
    }
}

void msh_on_input_char(msh_session_t *sess, char c)
{
    if (sess == NULL || sess->tty == NULL || !sess->running) {
        return;
    }

    if (c == '\r') {
        c = '\n';
    }

    if (c == '\n') {
        if (sess->line_len < sizeof(sess->line_buf)) {
            sess->line_buf[sess->line_len] = '\0';
        } else {
            sess->line_buf[sizeof(sess->line_buf) - 1u] = '\0';
        }
        msh_on_line_submit(sess, sess->line_buf);
        sess->line_len = 0;
        sess->line_buf[0] = '\0';
        return;
    }

    if (c == '\b' || c == 0x7f) {
        if (sess->line_len > 0u) {
            sess->line_len--;
            sess->line_buf[sess->line_len] = '\0';
            (void)tty_write(sess->tty, "\b \b", 3u);
        }
        return;
    }

    if (c >= 0x20 && sess->line_len + 1u < sizeof(sess->line_buf)) {
        sess->line_buf[sess->line_len++] = c;
        sess->line_buf[sess->line_len] = '\0';
        (void)tty_write(sess->tty, &c, 1u);
    }
}
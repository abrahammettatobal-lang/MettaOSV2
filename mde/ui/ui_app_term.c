#include <network.h>
#include <wifi.h>
/*
 * MettaOS — Terminal con shell integrada sobre el sistema de archivos en RAM.
 */
#include "ui_apps_common.h"

#include <interrupt/pit.h>

#define T_LINES   240
#define T_COLS    120
#define T_INPUT   200
#define T_HIST    24
#define T_PADX    14
#define T_TOP     (UI_TITLEBAR_H + 2)

enum { K_OUT = 0, K_PROMPT, K_ERR, K_DIM, K_ACCENT };

typedef struct {
    char    t[T_COLS + 1];
    uint8_t kind;
    uint8_t a, b; /* K_PROMPT: fin de usuario@host, fin del directorio */
} tline_t;

typedef struct {
    tline_t  lines[T_LINES];
    int      head, count;
    char     input[T_INPUT];
    int32_t  cur;
    int      cwd;
    int      scroll;       /* líneas desde abajo */
    char     hist[T_HIST][T_INPUT];
    int      nhist, hpos;
    int      ping_ticket;
    char     ping_target[T_INPUT];
    int      blink;
    int      cols;
    int32_t  caret_x, caret_y;
} term_t;

static term_t g_t[UI_MAX_WIN];

static int32_t cell_w(void) { return ui_text_width("M", UI_FONT_MONO); }
static int32_t line_h(void) { return ui_font_line(UI_FONT_MONO) + 3; }

static tline_t *push_line(term_t *s)
{
    tline_t *l;

    if (s->count < T_LINES) {
        l = &s->lines[(s->head + s->count) % T_LINES];
        s->count++;
    } else {
        l = &s->lines[s->head];
        s->head = (s->head + 1) % T_LINES;
    }
    memset(l, 0, sizeof(*l));
    return l;
}

static void out_kind(term_t *s, const char *txt, int kind)
{
    int cols = s->cols > 8 ? s->cols : 60;

    for (;;) {
        tline_t *l = push_line(s);
        int32_t i = 0, n = 0, chars = 0;

        l->kind = (uint8_t)kind;
        while (txt[i] && txt[i] != '\n' && chars < cols && n < T_COLS - 4) {
            int32_t j = i;
            ui_utf8_next(txt, &j);
            while (i < j)
                l->t[n++] = txt[i++];
            chars++;
        }
        l->t[n] = '\0';
        if (txt[i] == '\n')
            i++;
        else if (!txt[i])
            break;
        txt += i;
        if (!*txt)
            break;
    }
    s->scroll = 0;
}

static void out(term_t *s, const char *txt) { out_kind(s, txt, K_OUT); }
static void err(term_t *s, const char *txt) { out_kind(s, txt, K_ERR); }

static void dir_label(term_t *s, char *buf, size_t cap)
{
    char p[160];

    fs_path(s->cwd, p, sizeof(p));
    if (ui_str_starts(p, "/home/usuario")) {
        ui_strlcpy(buf, "~", cap);
        ui_strlcat(buf, p + 13, cap);
    } else {
        ui_strlcpy(buf, p, cap);
    }
}

static void prompt_line(term_t *s, const char *cmd)
{
    tline_t *l = push_line(s);
    char d[96];

    dir_label(s, d, sizeof(d));
    ui_strlcpy(l->t, "usuario@mettaos", sizeof(l->t));
    l->a = (uint8_t)strlen(l->t);
    ui_strlcat(l->t, " ", sizeof(l->t));
    ui_strlcat(l->t, d, sizeof(l->t));
    l->b = (uint8_t)strlen(l->t);
    ui_strlcat(l->t, " % ", sizeof(l->t));
    ui_strlcat(l->t, cmd, sizeof(l->t));
    l->kind = K_PROMPT;
}

static void term_open(ui_window_t *w, int slot, const char *arg)
{
    term_t *s = &g_t[slot];
    ui_time_t t;
    char buf[80], n[8];

    (void)arg;
    memset(s, 0, sizeof(*s));
    s->cwd = fs_home();
    s->cols = (w->w - 2 * T_PADX) / cell_w();
    ui_rtc_read(&t);
    ui_strlcpy(buf, "Último inicio de sesión: ", sizeof(buf));
    ui_fmt2(n, t.day);
    ui_strlcat(buf, n, sizeof(buf));
    ui_strlcat(buf, "/", sizeof(buf));
    ui_fmt2(n, t.month);
    ui_strlcat(buf, n, sizeof(buf));
    ui_strlcat(buf, " ", sizeof(buf));
    ui_fmt2(n, t.hour);
    ui_strlcat(buf, n, sizeof(buf));
    ui_strlcat(buf, ":", sizeof(buf));
    ui_fmt2(n, t.minute);
    ui_strlcat(buf, n, sizeof(buf));
    ui_strlcat(buf, " en ttys000", sizeof(buf));
    out_kind(s, buf, K_DIM);
    out_kind(s, "Escribe «help» para ver los comandos disponibles.", K_DIM);
    ui_strlcpy(w->title, "usuario — Terminal", sizeof(w->title));
}

/* ---------- comandos ---------- */

static int split(char *line, char **argv, int max)
{
    int n = 0;
    char *p = line;

    while (*p && n < max) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        if (*p == '"') {
            argv[n++] = ++p;
            while (*p && *p != '"')
                p++;
        } else {
            argv[n++] = p;
            while (*p && *p != ' ')
                p++;
        }
        if (*p)
            *p++ = '\0';
    }
    return n;
}

static int resolve(term_t *s, const char *path)
{
    char buf[160];

    if (path[0] == '~') {
        ui_strlcpy(buf, "/home/usuario", sizeof(buf));
        ui_strlcat(buf, path + 1, sizeof(buf));
        return fs_lookup(s->cwd, buf);
    }
    return fs_lookup(s->cwd, path);
}

static int create_path(term_t *s, const char *path, int type)
{
    char dir[160];
    const char *name = path;
    int parent = s->cwd, i, last = -1;

    for (i = 0; path[i]; i++)
        if (path[i] == '/')
            last = i;
    if (last >= 0) {
        int n = last < (int)sizeof(dir) - 1 ? last : (int)sizeof(dir) - 1;
        memcpy(dir, path, (size_t)n);
        dir[n] = '\0';
        parent = n == 0 ? fs_root() : resolve(s, dir);
        name = path + last + 1;
    }
    if (!fs_node(parent) || fs_node(parent)->type != FS_DIR || !name[0])
        return -1;
    i = fs_find(parent, name);
    if (i >= 0)
        return i;
    return fs_create(parent, name, type);
}

static void cmd_ls(term_t *s, int argc, char **argv)
{
    int d = argc > 1 ? resolve(s, argv[1]) : s->cwd;
    int f = -1, any = 0;
    char line[T_COLS + 1];
    int32_t col = 0;

    if (!fs_node(d)) {
        err(s, "ls: no existe el archivo o directorio");
        return;
    }
    if (fs_node(d)->type != FS_DIR) {
        out(s, fs_node(d)->name);
        return;
    }
    line[0] = '\0';
    while ((f = fs_child_next(d, f)) >= 0) {
        fs_node_t *n = fs_node(f);
        char item[64];
        int32_t len;

        ui_strlcpy(item, n->name, sizeof(item));
        if (n->type == FS_DIR)
            ui_strlcat(item, "/", sizeof(item));
        len = (int32_t)strlen(item);
        if (col + len + 2 > s->cols && col > 0) {
            out_kind(s, line, K_ACCENT);
            line[0] = '\0';
            col = 0;
        }
        ui_strlcat(line, item, sizeof(line));
        ui_strlcat(line, "   ", sizeof(line));
        col += len + 3;
        any = 1;
    }
    if (col > 0)
        out_kind(s, line, K_ACCENT);
    if (!any)
        out_kind(s, "(vacío)", K_DIM);
}

static void cmd_neofetch(term_t *s)
{
    static const char *art[] = {
        "   __  __      ",
        "  |  \\/  |     ",
        "  | \\  / |     ",
        "  | |\\/| |     ",
        "  | |  | |     ",
        "  |_|  |_|     ",
    };
    char info[6][64], n[24];
    uint64_t sec = pit_ticks_get() / 100u;
    int i;

    ui_strlcpy(info[0], "usuario@mettaos", 64);
    ui_strlcpy(info[1], "SO: MettaOS 1.0 Tahoe x86_64", 64);
    ui_strlcpy(info[2], "Kernel: metta 1.0 (Multiboot2)", 64);
    ui_strlcpy(info[3], "Tiempo activo: ", 64);
    ui_itoa((int64_t)(sec / 60u), n);
    ui_strlcat(info[3], n, 64);
    ui_strlcat(info[3], " min", 64);
    ui_strlcpy(info[4], "Escritorio: MDE · Liquid Glass", 64);
    ui_strlcpy(info[5], "Resolución: 1024 × 768", 64);
    for (i = 0; i < 6; i++) {
        char line[T_COLS + 1];
        ui_strlcpy(line, art[i], sizeof(line));
        ui_strlcat(line, info[i], sizeof(line));
        out_kind(s, line, i == 0 ? K_ACCENT : K_OUT);
    }
}

static void run(ui_window_t *w, term_t *s, const char *cmdline)
{
    char buf[T_INPUT], *argv[16], *redir = NULL;
    int argc, append = 0, i;
    char outbuf[512];

    ui_strlcpy(buf, cmdline, sizeof(buf));
    for (i = 0; buf[i]; i++) {
        if (buf[i] == '>') {
            buf[i] = '\0';
            if (buf[i + 1] == '>') {
                append = 1;
                i++;
            }
            redir = &buf[i + 1];
            while (*redir == ' ')
                redir++;
            break;
        }
    }
    argc = split(buf, argv, 16);
    if (argc == 0)
        return;

    if (ui_streq(argv[0], "help")) {
        out_kind(s, "Comandos disponibles:", K_ACCENT);
        out(s, "  ls [dir]        listar      cd [dir]       cambiar de carpeta");
        out(s, "  pwd             ruta actual cat <archivo>  mostrar contenido");
        out(s, "  echo <txt> [> archivo]      mkdir <dir>    crear carpeta");
        out(s, "  touch <archivo> crear       rm <ruta>      mover a la Papelera");
        out(s, "  open <ruta>     abrir       nano <archivo> editar en Notas");
        out(s, "  calc <expr>     calcular    date  uptime  uname  whoami");
        out(s, "  red             estado      dns [dominio]  consultar IPv4");
        out(s, "  ping <IP o dominio>         probar conectividad ICMP");
        out(s, "  wifi probe      buscar el Archer T3U Plus en USB");
        out(s, "  disco           guardar y ver el estado de MFS");
        out(s, "  bash            abrir GNU Bash");
        out(s, "  appimage <ruta> ejecutar un paquete de Aplicaciones");
        out(s, "  neofetch  history  clear  exit");
    } else if (ui_streq(argv[0], "bash")) {
        ui_open_app(APP_LINUX, NULL);
    } else if (ui_streq(argv[0], "appimage")) {
        int f = argc == 2 ? resolve(s, argv[1]) : -1;
        if (f < 0 && argc == 2) f = fs_find(fs_apps_dir(), argv[1]);
        if (fs_node(f) && fs_node(f)->type == FS_APPIMAGE) fs_open(f);
        else if (argc == 2 && argv[1][0] == '/') ui_open_app(APP_LINUX,argv[1]);
        else err(s, "Uso: appimage <archivo de /Aplicaciones>");
    } else if (ui_streq(argv[0], "disco")) {
        fs_sync(); out(s, fs_disk_status());
    } else if (ui_streq(argv[0], "wifi")) {
        if (argc == 2 && ui_streq(argv[1], "probe")) out(s, wifi_probe());
        else if (argc == 1) out(s, wifi_description());
        else err(s, "Uso: wifi [probe]");
    } else if (ui_streq(argv[0], "red")) {
        out(s, net_description());
    } else if (ui_streq(argv[0], "ping")) {
        const char *message;
        if (argc != 2) err(s, "Uso: ping <IPv4 o dominio>");
        else {
            int ticket = net_ping(argv[1], ui_now(), &message);
            if (!ticket) err(s, message);
            else {
                s->ping_ticket = ticket;
                ui_strlcpy(s->ping_target, argv[1], sizeof(s->ping_target));
                out(s, "Probando conectividad...");
            }
        }
    } else if (ui_streq(argv[0], "dns")) {
        if (argc > 1 && net_lookup(argv[1], ui_now()))
            out(s, "Consulta enviada. Escribe dns para ver el resultado.");
        else
            out(s, net_dns_result());
    } else if (ui_streq(argv[0], "clear")) {
        s->count = 0;
        s->head = 0;
    } else if (ui_streq(argv[0], "pwd")) {
        fs_path(s->cwd, outbuf, sizeof(outbuf));
        out(s, outbuf);
    } else if (ui_streq(argv[0], "cd")) {
        int d = argc > 1 ? resolve(s, argv[1]) : fs_home();
        if (!fs_node(d))
            err(s, "cd: no existe el directorio");
        else if (fs_node(d)->type != FS_DIR)
            err(s, "cd: no es un directorio");
        else
            s->cwd = d;
    } else if (ui_streq(argv[0], "ls")) {
        cmd_ls(s, argc, argv);
    } else if (ui_streq(argv[0], "cat")) {
        int f = argc > 1 ? resolve(s, argv[1]) : -1;
        if (!fs_node(f))
            err(s, "cat: no existe el archivo");
        else if (fs_node(f)->type == FS_DIR)
            err(s, "cat: es un directorio");
        else if (!fs_data(f))
            err(s, "cat: archivo binario");
        else
            out(s, fs_data(f));
    } else if (ui_streq(argv[0], "echo")) {
        outbuf[0] = '\0';
        for (i = 1; i < argc; i++) {
            if (i > 1)
                ui_strlcat(outbuf, " ", sizeof(outbuf));
            ui_strlcat(outbuf, argv[i], sizeof(outbuf));
        }
        if (redir && *redir) {
            int f = create_path(s, redir, FS_TEXT);
            char *d = fs_data(f);
            if (!d) {
                err(s, "echo: no se puede escribir el archivo");
            } else {
                if (!append)
                    d[0] = '\0';
                ui_strlcat(outbuf, "\n", sizeof(outbuf));
                if ((int)(strlen(d) + strlen(outbuf)) < FS_FILE_CAP - 1)
                    ui_strlcat(d, outbuf, FS_FILE_CAP);
                fs_node(f)->size = (int)strlen(d);
                fs_changed(f);
                ui_invalidate(0, 0, ui_screen_w(), ui_screen_h());
            }
        } else {
            out(s, outbuf);
        }
    } else if (ui_streq(argv[0], "mkdir") || ui_streq(argv[0], "touch")) {
        if (argc < 2)
            err(s, "uso: mkdir|touch <nombre>");
        for (i = 1; i < argc; i++)
            if (create_path(s, argv[i], argv[0][0] == 'm' ? FS_DIR : FS_TEXT) < 0)
                err(s, "no se pudo crear");
        ui_invalidate(0, 0, ui_screen_w(), ui_screen_h());
    } else if (ui_streq(argv[0], "rm")) {
        for (i = 1; i < argc; i++) {
            int f = resolve(s, argv[i]);
            if (!fs_node(f) || f == fs_root() || f == fs_home())
                err(s, "rm: no se puede eliminar");
            else if (fs_node(f)->parent == fs_trash())
                fs_remove(f);
            else
                fs_move(f, fs_trash());
        }
        ui_invalidate(0, 0, ui_screen_w(), ui_screen_h());
    } else if (ui_streq(argv[0], "open")) {
        int f = argc > 1 ? resolve(s, argv[1]) : s->cwd;
        if (!fs_node(f))
            err(s, "open: no existe");
        else
            fs_open(f);
    } else if (ui_streq(argv[0], "nano") || ui_streq(argv[0], "vim") || ui_streq(argv[0], "edit")) {
        int f = argc > 1 ? create_path(s, argv[1], FS_TEXT) : -1;
        if (f < 0 || !fs_data(f)) {
            err(s, "uso: nano <archivo.txt>");
        } else {
            char p[160];
            fs_path(f, p, sizeof(p));
            ui_open_app(APP_NOTES, p);
        }
    } else if (ui_streq(argv[0], "calc") || ui_streq(argv[0], "bc")) {
        double v;
        outbuf[0] = '\0';
        for (i = 1; i < argc; i++)
            ui_strlcat(outbuf, argv[i], sizeof(outbuf));
        if (argc < 2) {
            ui_open_app(APP_CALC, NULL);
        } else if (ui_eval(outbuf, &v)) {
            ui_fmt_num(v, outbuf);
            out_kind(s, outbuf, K_ACCENT);
        } else {
            err(s, "calc: expresión no válida");
        }
    } else if (ui_streq(argv[0], "date")) {
        static const char *days[] = { "dom", "lun", "mar", "mié", "jue", "vie", "sáb" };
        ui_time_t t;
        char n[8];
        ui_rtc_read(&t);
        ui_strlcpy(outbuf, days[t.wday % 7], sizeof(outbuf));
        ui_strlcat(outbuf, " ", sizeof(outbuf));
        ui_fmt2(n, t.day);
        ui_strlcat(outbuf, n, sizeof(outbuf));
        ui_strlcat(outbuf, "/", sizeof(outbuf));
        ui_fmt2(n, t.month);
        ui_strlcat(outbuf, n, sizeof(outbuf));
        ui_strlcat(outbuf, "/", sizeof(outbuf));
        ui_itoa(t.year, n);
        ui_strlcat(outbuf, n, sizeof(outbuf));
        ui_strlcat(outbuf, " ", sizeof(outbuf));
        ui_fmt2(n, t.hour);
        ui_strlcat(outbuf, n, sizeof(outbuf));
        ui_strlcat(outbuf, ":", sizeof(outbuf));
        ui_fmt2(n, t.minute);
        ui_strlcat(outbuf, n, sizeof(outbuf));
        ui_strlcat(outbuf, ":", sizeof(outbuf));
        ui_fmt2(n, t.second);
        ui_strlcat(outbuf, n, sizeof(outbuf));
        out(s, outbuf);
    } else if (ui_streq(argv[0], "uptime")) {
        uint64_t sec = pit_ticks_get() / 100u;
        char n[24];
        ui_strlcpy(outbuf, "activo desde hace ", sizeof(outbuf));
        ui_itoa((int64_t)(sec / 3600u), n);
        ui_strlcat(outbuf, n, sizeof(outbuf));
        ui_strlcat(outbuf, " h ", sizeof(outbuf));
        ui_itoa((int64_t)((sec / 60u) % 60u), n);
        ui_strlcat(outbuf, n, sizeof(outbuf));
        ui_strlcat(outbuf, " min ", sizeof(outbuf));
        ui_itoa((int64_t)(sec % 60u), n);
        ui_strlcat(outbuf, n, sizeof(outbuf));
        ui_strlcat(outbuf, " s", sizeof(outbuf));
        out(s, outbuf);
    } else if (ui_streq(argv[0], "uname")) {
        out(s, (argc > 1 && ui_streq(argv[1], "-a")) ? "MettaOS mettaos 1.0-tahoe x86_64" : "MettaOS");
    } else if (ui_streq(argv[0], "whoami")) {
        out(s, "usuario");
    } else if (ui_streq(argv[0], "hostname")) {
        out(s, "mettaos");
    } else if (ui_streq(argv[0], "neofetch")) {
        cmd_neofetch(s);
    } else if (ui_streq(argv[0], "history")) {
        for (i = 0; i < s->nhist; i++) {
            char n[8];
            ui_itoa(i + 1, n);
            ui_strlcpy(outbuf, "  ", sizeof(outbuf));
            ui_strlcat(outbuf, n, sizeof(outbuf));
            ui_strlcat(outbuf, "  ", sizeof(outbuf));
            ui_strlcat(outbuf, s->hist[i], sizeof(outbuf));
            out(s, outbuf);
        }
    } else if (ui_streq(argv[0], "exit") || ui_streq(argv[0], "logout")) {
        ui_close_window(w);
    } else {
        ui_strlcpy(outbuf, "Comando no encontrado: ", sizeof(outbuf));
        ui_strlcat(outbuf, argv[0], sizeof(outbuf));
        err(s, outbuf);
    }
}

/* ---------- dibujo ---------- */

static void term_draw(ui_window_t *w, int slot, ui_canvas_t *c, int32_t ox, int32_t oy, int focused)
{
    term_t *s = &g_t[slot];
    int32_t lh = line_h(), cw = cell_w();
    int rows = (w->h - T_TOP - 10) / lh, total, first, i;
    int32_t x0 = ox + T_PADX, y;
    char d[96];

    s->cols = (w->w - 2 * T_PADX) / cw;
    total = s->count + 1;
    s->caret_y = -1;
    first = total - rows - s->scroll;
    if (first < 0)
        first = 0;

    for (i = 0; i < rows && first + i < total; i++) {
        int idx = first + i;
        y = oy + T_TOP + i * lh;
        if (idx < s->count) {
            tline_t *l = &s->lines[(s->head + idx) % T_LINES];
            switch (l->kind) {
            case K_PROMPT: {
                int32_t x = ui_text_n(c, x0, y, l->t, l->a, UI_FONT_MONO, 0x30D158u, 255u);
                x = ui_text_n(c, x, y, l->t + l->a, l->b - l->a, UI_FONT_MONO, 0x64D2FFu, 255u);
                ui_text(c, x, y, l->t + l->b, UI_FONT_MONO, 0xF2F2F7u, 255u);
                break;
            }
            case K_ERR:    ui_text(c, x0, y, l->t, UI_FONT_MONO, 0xFF6961u, 255u); break;
            case K_DIM:    ui_text(c, x0, y, l->t, UI_FONT_MONO, 0x98989Fu, 255u); break;
            case K_ACCENT: ui_text(c, x0, y, l->t, UI_FONT_MONO, 0x64D2FFu, 255u); break;
            default:       ui_text(c, x0, y, l->t, UI_FONT_MONO, 0xE5E5EAu, 255u); break;
            }
        } else {
            int32_t x;
            dir_label(s, d, sizeof(d));
            x = ui_text(c, x0, y, "usuario@mettaos ", UI_FONT_MONO, 0x30D158u, 255u);
            x = ui_text(c, x, y, d, UI_FONT_MONO, 0x64D2FFu, 255u);
            x = ui_text(c, x, y, " % ", UI_FONT_MONO, 0xF2F2F7u, 255u);
            ui_text(c, x, y, s->input, UI_FONT_MONO, 0xF2F2F7u, 255u);
            s->caret_x = x - ox + ui_text_width_n(s->input, s->cur, UI_FONT_MONO);
            s->caret_y = y - oy;
            if (focused && !s->blink) {
                int32_t cx = x + ui_text_width_n(s->input, s->cur, UI_FONT_MONO);
                ui_fill(c, cx, y + 1, cw, lh - 3, 0xF2F2F7u, 200u);
                if (s->input[s->cur]) {
                    int32_t j = s->cur;
                    ui_utf8_next(s->input, &j);
                    ui_text_n(c, cx, y, s->input + s->cur, j - s->cur, UI_FONT_MONO, 0x1C1C1Eu, 255u);
                }
            } else if (!focused) {
                int32_t cx = x + ui_text_width_n(s->input, s->cur, UI_FONT_MONO);
                ui_rrect_stroke(c, (float)cx, (float)(y + 1), (float)cw, (float)(lh - 3), 1.0f, 1.0f,
                                0xF2F2F7u, 140u);
            }
        }
    }
}

/* ---------- entrada ---------- */

static void complete(term_t *s)
{
    static const char *cmds[] = { "help", "clear", "ls", "cd", "pwd", "cat", "echo", "mkdir",
                                  "touch", "rm", "open", "nano", "calc", "date", "uptime",
                                  "uname", "whoami", "hostname", "neofetch", "history", "exit",
                                  "bash", "appimage", "ping", "dns", "disco", "red" };
    int32_t start = s->cur, n, i;
    const char *match = NULL;
    int nmatch = 0, first_word = 1;
    char suffix[8] = "";

    while (start > 0 && s->input[start - 1] != ' ')
        start--;
    for (i = 0; i < start; i++)
        if (s->input[i] != ' ')
            first_word = 0;
    n = s->cur - start;

    if (first_word) {
        for (i = 0; i < (int)(sizeof(cmds) / sizeof(cmds[0])); i++) {
            int k, ok = 1;
            for (k = 0; k < n; k++)
                if (cmds[i][k] != s->input[start + k])
                    ok = 0;
            if (ok && (int)strlen(cmds[i]) >= n) {
                match = cmds[i];
                nmatch++;
            }
        }
        ui_strlcpy(suffix, " ", sizeof(suffix));
    } else {
        int f = -1;
        while ((f = fs_child_next(s->cwd, f)) >= 0) {
            fs_node_t *nd = fs_node(f);
            int k, ok = 1;
            for (k = 0; k < n; k++)
                if (nd->name[k] != s->input[start + k])
                    ok = 0;
            if (ok && (int)strlen(nd->name) >= n) {
                match = nd->name;
                nmatch++;
                ui_strlcpy(suffix, nd->type == FS_DIR ? "/" : " ", sizeof(suffix));
            }
        }
    }
    if (nmatch == 1 && match) {
        char rest[T_INPUT];
        ui_strlcpy(rest, s->input + s->cur, sizeof(rest));
        s->input[start] = '\0';
        ui_strlcat(s->input, match, sizeof(s->input));
        ui_strlcat(s->input, suffix, sizeof(s->input));
        s->cur = (int32_t)strlen(s->input);
        ui_strlcat(s->input, rest, sizeof(s->input));
    }
}

static void term_key(ui_window_t *w, int slot, const input_event_t *ev, const char *text)
{
    term_t *s = &g_t[slot];
    int32_t len = (int32_t)strlen(s->input);
    uint8_t mods = ev->u.key.modifiers;

    if ((mods & INPUT_MOD_CTRL) && ev->u.key.keycode == INPUT_KEY_L) {
        s->count = 0;
        s->head = 0;
    } else if ((mods & INPUT_MOD_CTRL) && ev->u.key.keycode == INPUT_KEY_C) {
        char tmp[T_INPUT + 4];
        ui_strlcpy(tmp, s->input, sizeof(tmp));
        ui_strlcat(tmp, "^C", sizeof(tmp));
        prompt_line(s, tmp);
        s->input[0] = '\0';
        s->cur = 0;
    } else if ((mods & INPUT_MOD_CTRL) && ev->u.key.keycode == INPUT_KEY_U) {
        s->input[0] = '\0';
        s->cur = 0;
    } else {
        switch (ev->u.key.keycode) {
        case INPUT_KEY_ENTER: {
            char cmd[T_INPUT];
            ui_strlcpy(cmd, s->input, sizeof(cmd));
            prompt_line(s, cmd);
            s->input[0] = '\0';
            s->cur = 0;
            if (cmd[0]) {
                if (s->nhist == T_HIST) {
                    int i;
                    for (i = 1; i < T_HIST; i++)
                        ui_strlcpy(s->hist[i - 1], s->hist[i], T_INPUT);
                    s->nhist--;
                }
                ui_strlcpy(s->hist[s->nhist++], cmd, T_INPUT);
            }
            s->hpos = s->nhist;
            run(w, s, cmd);
            if (!ui_window_by_slot(slot))
                return;
            break;
        }
        case INPUT_KEY_BACKSPACE:
            if (s->cur > 0) {
                int32_t p = ui_utf8_prev(s->input, s->cur);
                ui_memmove(s->input + p, s->input + s->cur, len - s->cur + 1);
                s->cur = p;
            }
            break;
        case INPUT_KEY_DELETE:
            if (s->cur < len) {
                int32_t j = s->cur;
                ui_utf8_next(s->input, &j);
                ui_memmove(s->input + s->cur, s->input + j, len - j + 1);
            }
            break;
        case INPUT_KEY_LEFT:
            s->cur = ui_utf8_prev(s->input, s->cur);
            break;
        case INPUT_KEY_RIGHT:
            if (s->cur < len)
                ui_utf8_next(s->input, &s->cur);
            break;
        case INPUT_KEY_HOME:
            s->cur = 0;
            break;
        case INPUT_KEY_END:
            s->cur = len;
            break;
        case INPUT_KEY_UP:
            if (s->hpos > 0) {
                s->hpos--;
                ui_strlcpy(s->input, s->hist[s->hpos], sizeof(s->input));
                s->cur = (int32_t)strlen(s->input);
            }
            break;
        case INPUT_KEY_DOWN:
            if (s->hpos < s->nhist) {
                s->hpos++;
                if (s->hpos == s->nhist)
                    s->input[0] = '\0';
                else
                    ui_strlcpy(s->input, s->hist[s->hpos], sizeof(s->input));
                s->cur = (int32_t)strlen(s->input);
            }
            break;
        case INPUT_KEY_TAB:
            complete(s);
            break;
        case INPUT_KEY_PAGEUP:
            s->scroll = ui_min(s->scroll + 10, s->count);
            ui_win_invalidate(w);
            return;
        case INPUT_KEY_PAGEDOWN:
            s->scroll = ui_max(s->scroll - 10, 0);
            ui_win_invalidate(w);
            return;
        default: {
            int32_t n = (int32_t)strlen(text);
            if (n == 0 || len + n >= T_INPUT - 1)
                return;
            ui_memmove(s->input + s->cur + n, s->input + s->cur, len - s->cur + 1);
            memcpy(s->input + s->cur, text, (size_t)n);
            s->cur += n;
            break;
        }
        }
    }
    s->scroll = 0;
    s->blink = 0;
    ui_win_invalidate(w);
}

static void term_mouse(ui_window_t *w, int slot, int type, int32_t lx, int32_t ly, int btn, int clicks)
{
    (void)w;
    (void)slot;
    (void)lx;
    (void)btn;
    (void)clicks;
    if (type == UI_M_DOWN && ly >= UI_TITLEBAR_H)
        ui_consume();
}

static void term_wheel(ui_window_t *w, int slot, int delta)
{
    term_t *s = &g_t[slot];

    s->scroll = ui_clampi(s->scroll - delta * 3, 0, s->count);
    ui_win_invalidate(w);
}

static int term_tick(ui_window_t *w, int slot, uint64_t now)
{
    term_t *s = &g_t[slot];
    int ph = (int)((now / 530u) % 2u);

    if (s->ping_ticket) {
        unsigned rtt;
        const char *message = net_ping_result(s->ping_ticket, &rtt);
        if (message) {
            char line[T_INPUT + 64], number[24];
            ui_strlcpy(line, s->ping_target, sizeof(line));
            ui_strlcat(line, ": ", sizeof(line));
            ui_strlcat(line, message, sizeof(line));
            if (ui_streq(message, "Respuesta")) {
                ui_itoa(rtt, number);
                ui_strlcat(line, " en ", sizeof(line));
                ui_strlcat(line, number, sizeof(line));
                ui_strlcat(line, " ms", sizeof(line));
            }
            out(s, line);
            s->ping_ticket = 0;
            ui_win_invalidate(w);
            return 1;
        }
    }
    if (!ui_win_focused(w) || ph == s->blink)
        return 0;
    s->blink = ph;
    if (s->caret_y >= 0)
        ui_win_invalidate_local(w, s->caret_x - 1, s->caret_y, cell_w()+2, line_h());
    return 1;
}

const ui_app_t app_terminal = {
    "Consola MDE", UI_ICON_TERMINAL, 680, 430, 380, 220, 0x172B34u, 0, 1, 1, 0,
    term_open, term_draw, term_mouse, term_key, term_wheel, term_tick, NULL, NULL,
};

/*
 * MettaOS — controlador PS/2 (teclado set 1 + ratón). SA-13
 * Solo polling no bloqueante; IRQ 1/12 opcional vía input_poll desde el compositor.
 */
#include <input/event_queue.h>
#include <input/input.h>
#include <input/ps2.h>

#include "interrupt/pit.h"
#include "interrupt/ports.h"

#define PS2_DATA   0x60u
#define PS2_STATUS 0x64u
#define PS2_CMD    0x64u

#define PS2_STATUS_OUT_FULL 0x01u
#define PS2_STATUS_AUX      0x20u

#define PS2_MOUSE_SCALE 1

static int       g_ready;
static uint8_t   g_modifiers;
static int32_t   g_mouse_x;
static int32_t   g_mouse_y;
static int32_t   g_screen_w;
static int32_t   g_screen_h;
static uint8_t   g_mouse_btns;

static uint8_t   g_kbd_ext;
static uint8_t   g_mouse_packet[3];
static uint8_t   g_mouse_idx;

/* VMware/QEMU vmmouse: puntero absoluto vía backdoor (QEMU vmport=on por defecto). */
#define VMMOUSE_PORT            0x5658u
#define VMMOUSE_MAGIC           0x564D5868u
#define VMMOUSE_CMD_GETVERSION  10u
#define VMMOUSE_CMD_DATA        39u
#define VMMOUSE_CMD_STATUS      40u
#define VMMOUSE_CMD_COMMAND     41u
#define VMMOUSE_ENABLE          0x45414552u
#define VMMOUSE_REQ_ABSOLUTE    0x53424152u
#define VMMOUSE_VERSION_ID      0x3442554Au
#define VMMOUSE_BTN_LEFT        0x20u
#define VMMOUSE_BTN_RIGHT       0x10u
#define VMMOUSE_BTN_MIDDLE      0x08u

static int g_vmmouse;

typedef struct {
    uint32_t a, b, c, d;
} vmmouse_regs_t;

static void vmmouse_call(uint32_t cmd, uint32_t arg, vmmouse_regs_t *r)
{
    uint32_t a = VMMOUSE_MAGIC, b = arg, c = cmd, d = VMMOUSE_PORT;

    __asm__ volatile("inl %%dx, %%eax"
                     : "+a"(a), "+b"(b), "+c"(c), "+d"(d)
                     :
                     : "memory");
    r->a = a;
    r->b = b;
    r->c = c;
    r->d = d;
}

static int vmmouse_init(void)
{
    vmmouse_regs_t r;

    vmmouse_call(VMMOUSE_CMD_GETVERSION, 0u, &r);
    if (r.b != VMMOUSE_MAGIC || r.a == 0xFFFFFFFFu)
        return -1;

    vmmouse_call(VMMOUSE_CMD_COMMAND, VMMOUSE_ENABLE, &r);
    vmmouse_call(VMMOUSE_CMD_STATUS, 0u, &r);
    if ((r.a & 0x0000FFFFu) == 0u)
        return -1;
    vmmouse_call(VMMOUSE_CMD_DATA, 1u, &r);
    if (r.a != VMMOUSE_VERSION_ID)
        return -1;

    vmmouse_call(VMMOUSE_CMD_COMMAND, VMMOUSE_REQ_ABSOLUTE, &r);
    return 0;
}

static int ps2_wait_write(void)
{
    for (uint32_t i = 0; i < 100000u; i++) {
        if ((inb(PS2_STATUS) & 0x02u) == 0) {
            return 0;
        }
        io_wait();
    }
    return -1;
}

static int ps2_wait_read(void)
{
    for (uint32_t i = 0; i < 100000u; i++) {
        if (inb(PS2_STATUS) & PS2_STATUS_OUT_FULL) {
            return 0;
        }
        io_wait();
    }
    return -1;
}

static int ps2_write_cmd(uint8_t cmd)
{
    if (ps2_wait_write() != 0) {
        return -1;
    }
    outb(PS2_CMD, cmd);
    return 0;
}

static int ps2_write_data(uint8_t data)
{
    if (ps2_wait_write() != 0) {
        return -1;
    }
    outb(PS2_DATA, data);
    return 0;
}

static int ps2_write_aux(uint8_t data)
{
    if (ps2_write_cmd(0xD4u) != 0) {
        return -1;
    }
    return ps2_write_data(data);
}

static uint8_t ps2_read_data(void)
{
    (void)ps2_wait_read();
    return inb(PS2_DATA);
}

static void ps2_flush_output(void)
{
    for (uint32_t i = 0; i < 16u; i++) {
        if ((inb(PS2_STATUS) & PS2_STATUS_OUT_FULL) == 0) {
            break;
        }
        (void)inb(PS2_DATA);
    }
}

static input_event_t ps2_make_event(input_event_type_t type)
{
    input_event_t ev;
    ev.type     = type;
    ev.time_ms  = pit_ticks_get() * (1000ull / PIT_TICK_HZ);
    ev.u.key.keycode    = INPUT_KEY_UNKNOWN;
    ev.u.key.scancode   = 0;
    ev.u.key.modifiers  = g_modifiers;
    ev.u.mouse.x        = 0;
    ev.u.mouse.y        = 0;
    ev.u.mouse.dx       = 0;
    ev.u.mouse.dy       = 0;
    ev.u.btn.button     = INPUT_BTN_LEFT;
    ev.u.btn.wheel_delta = 0;
    return ev;
}

static input_keycode_t ps2_scancode_to_key(uint8_t sc, int extended)
{
    if (extended) {
        switch (sc) {
        case 0x48:
            return INPUT_KEY_UP;
        case 0x50:
            return INPUT_KEY_DOWN;
        case 0x4B:
            return INPUT_KEY_LEFT;
        case 0x4D:
            return INPUT_KEY_RIGHT;
        case 0x47:
            return INPUT_KEY_HOME;
        case 0x4F:
            return INPUT_KEY_END;
        case 0x49:
            return INPUT_KEY_PAGEUP;
        case 0x51:
            return INPUT_KEY_PAGEDOWN;
        case 0x52:
            return INPUT_KEY_INSERT;
        case 0x53:
            return INPUT_KEY_DELETE;
        case 0x1D:
            return INPUT_KEY_RIGHTCTRL;
        case 0x38:
            return INPUT_KEY_RIGHTALT;
        default:
            return INPUT_KEY_UNKNOWN;
        }
    }

    switch (sc) {
    case 0x01:
        return INPUT_KEY_ESC;
    case 0x02:
        return INPUT_KEY_1;
    case 0x03:
        return INPUT_KEY_2;
    case 0x04:
        return INPUT_KEY_3;
    case 0x05:
        return INPUT_KEY_4;
    case 0x06:
        return INPUT_KEY_5;
    case 0x07:
        return INPUT_KEY_6;
    case 0x08:
        return INPUT_KEY_7;
    case 0x09:
        return INPUT_KEY_8;
    case 0x0A:
        return INPUT_KEY_9;
    case 0x0B:
        return INPUT_KEY_0;
    case 0x0C:
        return INPUT_KEY_MINUS;
    case 0x0D:
        return INPUT_KEY_EQUAL;
    case 0x0E:
        return INPUT_KEY_BACKSPACE;
    case 0x0F:
        return INPUT_KEY_TAB;
    case 0x10:
        return INPUT_KEY_Q;
    case 0x11:
        return INPUT_KEY_W;
    case 0x12:
        return INPUT_KEY_E;
    case 0x13:
        return INPUT_KEY_R;
    case 0x14:
        return INPUT_KEY_T;
    case 0x15:
        return INPUT_KEY_Y;
    case 0x16:
        return INPUT_KEY_U;
    case 0x17:
        return INPUT_KEY_I;
    case 0x18:
        return INPUT_KEY_O;
    case 0x19:
        return INPUT_KEY_P;
    case 0x1A:
        return INPUT_KEY_LEFTBRACE;
    case 0x1B:
        return INPUT_KEY_RIGHTBRACE;
    case 0x1C:
        return INPUT_KEY_ENTER;
    case 0x1D:
        return INPUT_KEY_LEFTCTRL;
    case 0x1E:
        return INPUT_KEY_A;
    case 0x1F:
        return INPUT_KEY_S;
    case 0x20:
        return INPUT_KEY_D;
    case 0x21:
        return INPUT_KEY_F;
    case 0x22:
        return INPUT_KEY_G;
    case 0x23:
        return INPUT_KEY_H;
    case 0x24:
        return INPUT_KEY_J;
    case 0x25:
        return INPUT_KEY_K;
    case 0x26:
        return INPUT_KEY_L;
    case 0x27:
        return INPUT_KEY_SEMICOLON;
    case 0x28:
        return INPUT_KEY_APOSTROPHE;
    case 0x29:
        return INPUT_KEY_GRAVE;
    case 0x2A:
        return INPUT_KEY_LEFTSHIFT;
    case 0x2B:
        return INPUT_KEY_BACKSLASH;
    case 0x2C:
        return INPUT_KEY_Z;
    case 0x2D:
        return INPUT_KEY_X;
    case 0x2E:
        return INPUT_KEY_C;
    case 0x2F:
        return INPUT_KEY_V;
    case 0x30:
        return INPUT_KEY_B;
    case 0x31:
        return INPUT_KEY_N;
    case 0x32:
        return INPUT_KEY_M;
    case 0x33:
        return INPUT_KEY_COMMA;
    case 0x34:
        return INPUT_KEY_DOT;
    case 0x35:
        return INPUT_KEY_SLASH;
    case 0x36:
        return INPUT_KEY_RIGHTSHIFT;
    case 0x38:
        return INPUT_KEY_LEFTALT;
    case 0x39:
        return INPUT_KEY_SPACE;
    case 0x3A:
        return INPUT_KEY_CAPSLOCK;
    case 0x3B:
        return INPUT_KEY_F1;
    case 0x3C:
        return INPUT_KEY_F2;
    case 0x3D:
        return INPUT_KEY_F3;
    case 0x3E:
        return INPUT_KEY_F4;
    case 0x3F:
        return INPUT_KEY_F5;
    case 0x40:
        return INPUT_KEY_F6;
    case 0x41:
        return INPUT_KEY_F7;
    case 0x42:
        return INPUT_KEY_F8;
    case 0x43:
        return INPUT_KEY_F9;
    case 0x44:
        return INPUT_KEY_F10;
    case 0x57:
        return INPUT_KEY_F11;
    case 0x58:
        return INPUT_KEY_F12;
    default:
        return INPUT_KEY_UNKNOWN;
    }
}

static void ps2_update_modifier(input_keycode_t key, int down)
{
    uint8_t mask = 0;

    switch (key) {
    case INPUT_KEY_LEFTSHIFT:
    case INPUT_KEY_RIGHTSHIFT:
        mask = INPUT_MOD_SHIFT;
        break;
    case INPUT_KEY_LEFTCTRL:
    case INPUT_KEY_RIGHTCTRL:
        mask = INPUT_MOD_CTRL;
        break;
    case INPUT_KEY_LEFTALT:
    case INPUT_KEY_RIGHTALT:
        mask = INPUT_MOD_ALT;
        break;
    default:
        return;
    }

    if (mask == 0) {
        return;
    }
    if (down) {
        g_modifiers |= mask;
    } else {
        g_modifiers &= (uint8_t)~mask;
    }
}

static void ps2_push_key_event(input_event_type_t type, uint8_t sc, input_keycode_t key)
{
    input_event_t ev = ps2_make_event(type);
    ev.u.key.scancode  = sc;
    ev.u.key.keycode   = key;
    ev.u.key.modifiers = g_modifiers;
    (void)event_queue_push(&ev);
}

static void ps2_handle_keyboard_byte(uint8_t data)
{
    int release;
    uint8_t sc;
    int extended;

    if (data == 0xE0u) {
        g_kbd_ext = 1;
        return;
    }
    if (data == 0xE1u) {
        g_kbd_ext = 0;
        return;
    }

    release  = (data & 0x80u) != 0;
    sc       = release ? (uint8_t)(data & 0x7Fu) : data;
    extended = g_kbd_ext;
    g_kbd_ext = 0;

    input_keycode_t key = ps2_scancode_to_key(sc, extended);
    if (key == INPUT_KEY_UNKNOWN && (extended || sc != 0x56u)) {
        return;
    }

    ps2_update_modifier(key, !release);
    ps2_push_key_event(release ? INPUT_EV_KEY_UP : INPUT_EV_KEY_DOWN, sc, key);
}

static int32_t ps2_clamp_x(int32_t v)
{
    if (g_screen_w > 0) {
        if (v < 0) {
            return 0;
        }
        if (v >= g_screen_w) {
            return g_screen_w - 1;
        }
    }
    return v;
}

static int32_t ps2_clamp_y(int32_t v)
{
    if (g_screen_h > 0) {
        if (v < 0) {
            return 0;
        }
        if (v >= g_screen_h) {
            return g_screen_h - 1;
        }
    }
    return v;
}

void ps2_set_screen_bounds(int32_t width, int32_t height)
{
    g_screen_w = (width > 0) ? width : 0;
    g_screen_h = (height > 0) ? height : 0;
    g_mouse_x  = ps2_clamp_x(g_mouse_x);
    g_mouse_y  = ps2_clamp_y(g_mouse_y);
}

void ps2_set_position(int32_t x, int32_t y)
{
    g_mouse_x = ps2_clamp_x(x);
    g_mouse_y = ps2_clamp_y(y);
}

void ps2_get_position(int32_t *x, int32_t *y)
{
    if (x) {
        *x = g_mouse_x;
    }
    if (y) {
        *y = g_mouse_y;
    }
}

static void ps2_emit_mouse_move(int32_t dx, int32_t dy)
{
    if (dx == 0 && dy == 0) {
        return;
    }

    dx *= PS2_MOUSE_SCALE;
    dy *= PS2_MOUSE_SCALE;

    g_mouse_x = ps2_clamp_x(g_mouse_x + dx);
    g_mouse_y = ps2_clamp_y(g_mouse_y + dy);

    input_event_t ev = ps2_make_event(INPUT_EV_MOUSE_MOVE);
    ev.u.mouse.x  = g_mouse_x;
    ev.u.mouse.y  = g_mouse_y;
    ev.u.mouse.dx = dx;
    ev.u.mouse.dy = dy;
    (void)event_queue_push(&ev);
}

static void ps2_emit_mouse_button(input_mouse_button_t btn, int down)
{
    input_event_t ev =
        ps2_make_event(down ? INPUT_EV_MOUSE_BUTTON_DOWN : INPUT_EV_MOUSE_BUTTON_UP);
    ev.u.btn.button = btn;
    (void)event_queue_push(&ev);
}

static void vmmouse_poll(void)
{
    uint32_t guard;

    for (guard = 0; guard < 32u; guard++) {
        vmmouse_regs_t r;
        uint32_t status;
        uint32_t buttons;
        int32_t  x, y;
        int8_t   wheel;
        uint8_t  mapped;
        uint8_t  i;

        vmmouse_call(VMMOUSE_CMD_STATUS, 0u, &r);
        status = r.a;
        if ((status & 0xFFFF0000u) == 0xFFFF0000u) {
            vmmouse_call(VMMOUSE_CMD_COMMAND, VMMOUSE_ENABLE, &r);
            vmmouse_call(VMMOUSE_CMD_COMMAND, VMMOUSE_REQ_ABSOLUTE, &r);
            return;
        }
        if ((status & 0x0000FFFFu) < 4u)
            return;

        vmmouse_call(VMMOUSE_CMD_DATA, 4u, &r);
        buttons = r.a;
        x = (int32_t)((r.b & 0xFFFFu) * (uint32_t)(g_screen_w > 0 ? g_screen_w : 1) / 65536u);
        y = (int32_t)((r.c & 0xFFFFu) * (uint32_t)(g_screen_h > 0 ? g_screen_h : 1) / 65536u);
        wheel = (int8_t)(r.d & 0xFFu);

        x = ps2_clamp_x(x);
        y = ps2_clamp_y(y);
        if (x != g_mouse_x || y != g_mouse_y) {
            input_event_t ev = ps2_make_event(INPUT_EV_MOUSE_MOVE);
            ev.u.mouse.dx = x - g_mouse_x;
            ev.u.mouse.dy = y - g_mouse_y;
            g_mouse_x = x;
            g_mouse_y = y;
            ev.u.mouse.x = x;
            ev.u.mouse.y = y;
            (void)event_queue_push(&ev);
        }

        mapped = 0;
        if (buttons & VMMOUSE_BTN_LEFT)
            mapped |= 1u;
        if (buttons & VMMOUSE_BTN_RIGHT)
            mapped |= 2u;
        if (buttons & VMMOUSE_BTN_MIDDLE)
            mapped |= 4u;
        for (i = 0; i < 3u; i++) {
            uint8_t mask = (uint8_t)(1u << i);
            if ((mapped & mask) != 0u && (g_mouse_btns & mask) == 0u)
                ps2_emit_mouse_button((input_mouse_button_t)i, 1);
            else if ((mapped & mask) == 0u && (g_mouse_btns & mask) != 0u)
                ps2_emit_mouse_button((input_mouse_button_t)i, 0);
        }
        g_mouse_btns = mapped;

        if (wheel != 0) {
            input_event_t ev = ps2_make_event(INPUT_EV_MOUSE_WHEEL);
            ev.u.btn.button = INPUT_BTN_MIDDLE;
            ev.u.btn.wheel_delta = wheel;
            (void)event_queue_push(&ev);
        }
    }
}

static void ps2_handle_mouse_byte(uint8_t data)
{
    /* Bit 3 del primer byte siempre 1 en paquetes válidos: resync si hace falta. */
    if (g_mouse_idx == 0u && (data & 0x08u) == 0u) {
        return;
    }

    g_mouse_packet[g_mouse_idx++] = data;
    if (g_mouse_idx < 3u) {
        return;
    }
    g_mouse_idx = 0;

    uint8_t flags = g_mouse_packet[0];
    /* Movimiento: byte como unsigned + bit de signo (no mezclar con cast int8). */
    int32_t dx = (int32_t)g_mouse_packet[1];
    int32_t dy = (int32_t)g_mouse_packet[2];

    if (flags & 0x10u) {
        dx -= 256;
    }
    if (flags & 0x20u) {
        dy -= 256;
    }
    /* PS/2 Y+ hacia arriba; pantalla Y+ hacia abajo. */
    dy = -dy;

    if (dx != 0 || dy != 0)
        ps2_emit_mouse_move(dx, dy);

    /* con vmmouse, QEMU inyecta paquetes PS/2 falsos sin botones: los botones
     * llegan sólo por el backdoor */
    if (!g_vmmouse) {
        uint8_t buttons = flags & 0x07u;
        uint8_t i;

        for (i = 0; i < 3u; i++) {
            uint8_t mask = (uint8_t)(1u << i);
            if ((buttons & mask) != 0u && (g_mouse_btns & mask) == 0u) {
                ps2_emit_mouse_button((input_mouse_button_t)i, 1);
            } else if ((buttons & mask) == 0u && (g_mouse_btns & mask) != 0u) {
                ps2_emit_mouse_button((input_mouse_button_t)i, 0);
            }
        }
        g_mouse_btns = buttons;
    }
}

static void ps2_drain_once(void)
{
    uint8_t status = inb(PS2_STATUS);
    if ((status & PS2_STATUS_OUT_FULL) == 0) {
        return;
    }

    uint8_t data = inb(PS2_DATA);
    if (status & PS2_STATUS_AUX) {
        ps2_handle_mouse_byte(data);
    } else {
        ps2_handle_keyboard_byte(data);
    }
}

int ps2_init(void)
{
    g_ready      = 0;
    g_modifiers  = INPUT_MOD_NONE;
    g_mouse_x    = 0;
    g_mouse_y    = 0;
    g_mouse_btns = 0;
    g_kbd_ext    = 0;
    g_mouse_idx  = 0;

    ps2_flush_output();

    (void)ps2_write_cmd(0xADu); /* disable kbd */
    (void)ps2_write_cmd(0xA7u); /* disable mouse */
    ps2_flush_output();

    (void)ps2_write_cmd(0x20u);
    (void)ps2_wait_read();
    {
        uint8_t cfg = ps2_read_data();
        /* IRQ1/IRQ12 off (polling); traducción a set 1 on; relojes on. */
        cfg &= (uint8_t)~(0x03u | 0x10u | 0x20u);
        cfg |= 0x40u;
        (void)ps2_write_cmd(0x60u);
        (void)ps2_write_data(cfg);
    }

    (void)ps2_write_cmd(0xAEu); /* enable kbd */
    (void)ps2_write_cmd(0xA8u); /* enable mouse */

    (void)ps2_write_data(0xF4u); /* kbd enable scanning */
    (void)ps2_wait_read();
    (void)ps2_read_data();

    (void)ps2_write_aux(0xFFu); /* mouse reset */
    (void)ps2_wait_read();
    (void)ps2_read_data(); /* ACK */
    (void)ps2_wait_read();
    (void)ps2_read_data(); /* BAT */
    (void)ps2_wait_read();
    (void)ps2_read_data(); /* ID */

    (void)ps2_write_aux(0xF6u); /* defaults */
    (void)ps2_wait_read();
    (void)ps2_read_data();

    (void)ps2_write_aux(0xF4u); /* enable data reporting */
    (void)ps2_wait_read();
    (void)ps2_read_data();

    g_vmmouse = (vmmouse_init() == 0);

    g_ready = 1;
    return 0;
}

void ps2_poll(void)
{
    if (!g_ready) {
        return;
    }

    for (uint32_t n = 0; n < 64u; n++) {
        uint8_t status = inb(PS2_STATUS);
        if ((status & PS2_STATUS_OUT_FULL) == 0) {
            break;
        }
        ps2_drain_once();
    }

    if (g_vmmouse)
        vmmouse_poll();
}
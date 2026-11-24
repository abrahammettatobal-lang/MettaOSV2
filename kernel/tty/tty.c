/*
 * MettaOS — dispositivos TTY en kernel (SA-28).
 */
#include "tty/tty.h"

#include <stdint.h>

#include "serial.h"

struct tty_device {
    tty_id_t  id;
    char      name[8];
    uint8_t   input_ring[TTY_INPUT_RING_SIZE];
    size_t    in_head;
    size_t    in_tail;
    uint8_t   output_ring[TTY_OUTPUT_RING_SIZE];
    size_t    out_head;
    size_t    out_tail;
    int       echo;
    int       active;
};

static tty_device_t g_ttys[TTY_MAX_DEVICES];
static int          g_tty_ready;
static int          g_serial_mirror = 1;

static size_t ring_count(size_t head, size_t tail, size_t cap)
{
    if (head >= tail) {
        return head - tail;
    }
    return cap - tail + head;
}

static size_t ring_space(size_t head, size_t tail, size_t cap)
{
    return cap - 1u - ring_count(head, tail, cap);
}

static void ring_push(uint8_t *ring, size_t cap, size_t *head, uint8_t byte)
{
    size_t next = (*head + 1u) % cap;
    if (next == *head) {
        return;
    }
    ring[*head] = byte;
    *head = next;
}

static int ring_pop(uint8_t *ring, size_t cap, size_t *head, size_t *tail, uint8_t *out)
{
    if (*tail == *head) {
        return 0;
    }
    *out = ring[*tail];
    *tail = (*tail + 1u) % cap;
    return 1;
}

static void tty_mirror_serial(const char *buf, size_t len)
{
    size_t i;

    if (!g_serial_mirror) {
        return;
    }
    for (i = 0; i < len; i++) {
        serial_putc(buf[i]);
    }
}

void tty_init(void)
{
    tty_id_t i;

    if (g_tty_ready) {
        return;
    }

    for (i = 0; i < TTY_MAX_DEVICES; i++) {
        tty_device_t *t = &g_ttys[i];

        t->id = i;
        t->name[0] = 't';
        t->name[1] = 't';
        t->name[2] = 'y';
        t->name[3] = (char)('0' + (int)i);
        t->name[4] = '\0';
        t->in_head = t->in_tail = 0;
        t->out_head = t->out_tail = 0;
        t->echo = 1;
        t->active = (i == TTY_VT_DEFAULT) ? 1 : 0;
    }

    g_tty_ready = 1;
}

tty_device_t *tty_get(tty_id_t id)
{
    if (!g_tty_ready || id >= TTY_MAX_DEVICES) {
        return NULL;
    }
    return &g_ttys[id];
}

tty_device_t *tty_default(void)
{
    if (!g_tty_ready) {
        tty_init();
    }
    return tty_get(TTY_VT_DEFAULT);
}

size_t tty_write(tty_device_t *tty, const char *buf, size_t len)
{
    size_t i;
    size_t written = 0;

    if (tty == NULL || buf == NULL || len == 0u) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        if (ring_space(tty->out_head, tty->out_tail, TTY_OUTPUT_RING_SIZE) == 0u) {
            break;
        }
        ring_push(tty->output_ring, TTY_OUTPUT_RING_SIZE, &tty->out_head, (uint8_t)buf[i]);
        written++;
    }

    if (written > 0u) {
        tty_mirror_serial(buf, written);
    }

    return written;
}

size_t tty_read(tty_device_t *tty, char *buf, size_t max_len)
{
    size_t n = 0;
    uint8_t byte;

    if (tty == NULL || buf == NULL || max_len == 0u) {
        return 0;
    }

    while (n < max_len && ring_pop(tty->input_ring, TTY_INPUT_RING_SIZE,
                                   &tty->in_head, &tty->in_tail, &byte)) {
        buf[n++] = (char)byte;
    }

    return n;
}

void tty_input_byte(tty_device_t *tty, uint8_t byte)
{
    char echo_buf[2];

    if (tty == NULL) {
        return;
    }

    if (ring_space(tty->in_head, tty->in_tail, TTY_INPUT_RING_SIZE) == 0u) {
        return;
    }

    ring_push(tty->input_ring, TTY_INPUT_RING_SIZE, &tty->in_head, byte);

    if (tty->echo && byte >= 0x20u && byte != 0x7fu) {
        echo_buf[0] = (char)byte;
        echo_buf[1] = '\0';
        (void)tty_write(tty, echo_buf, 1u);
    } else if (tty->echo && byte == '\n') {
        (void)tty_write(tty, "\r\n", 2u);
    }
}

size_t tty_drain_output(tty_device_t *tty, char *buf, size_t max_len)
{
    size_t n = 0;
    uint8_t byte;

    if (tty == NULL || buf == NULL || max_len == 0u) {
        return 0;
    }

    while (n < max_len && ring_pop(tty->output_ring, TTY_OUTPUT_RING_SIZE,
                                    &tty->out_head, &tty->out_tail, &byte)) {
        buf[n++] = (char)byte;
    }

    return n;
}

void tty_set_serial_mirror(int enable)
{
    g_serial_mirror = enable ? 1 : 0;
}
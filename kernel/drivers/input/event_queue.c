/*
 * MettaOS — cola de eventos + fachada Input ↔ MDE. SA-13
 */
#include <input/event_queue.h>
#include <input/input.h>
#include <input/ps2.h>

static input_event_t g_queue[INPUT_QUEUE_CAPACITY];
static size_t        g_head;
static size_t        g_tail;
static size_t        g_count;

void event_queue_reset(void)
{
    g_head  = 0;
    g_tail  = 0;
    g_count = 0;
}

int event_queue_push(const input_event_t *ev)
{
    if (!ev || ev->type == INPUT_EV_NONE) {
        return -1;
    }
    if (g_count >= INPUT_QUEUE_CAPACITY) {
        return -2;
    }
    g_queue[g_tail] = *ev;
    g_tail          = (g_tail + 1u) % INPUT_QUEUE_CAPACITY;
    g_count++;
    return 0;
}

int event_queue_pop(input_event_t *out)
{
    if (!out || g_count == 0) {
        return -1;
    }
    *out   = g_queue[g_head];
    g_head = (g_head + 1u) % INPUT_QUEUE_CAPACITY;
    g_count--;
    return 0;
}

size_t event_queue_pending(void)
{
    return g_count;
}

int input_init(void)
{
    event_queue_reset();
    return ps2_init();
}

void input_poll(void)
{
    ps2_poll();
}

int input_pop(input_event_t *out)
{
    return event_queue_pop(out);
}

size_t input_pending(void)
{
    return event_queue_pending();
}
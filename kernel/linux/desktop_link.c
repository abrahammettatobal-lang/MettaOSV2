#include <linux/desktop_link.h>
#include <interrupt/ports.h>
#include <string.h>

/* Transitional PCI virtio-console, one port, split rings with no interrupts. */
#define QUEUE_MAX 256
#define PAGE 4096u
#define RX_BYTES 1024u

struct descriptor { uint64_t address; uint32_t length; uint16_t flags, next; };
struct avail_ring { uint16_t flags, index, ring[QUEUE_MAX]; };
struct used_ring { uint16_t flags, index; struct { uint32_t id, size; } ring[QUEUE_MAX]; };
struct queue {
    uint8_t pages[3 * PAGE] __attribute__((aligned(PAGE)));
    volatile struct descriptor *desc;
    volatile struct avail_ring *avail;
    volatile struct used_ring *used;
    uint16_t count, taken;
};
static struct queue rx, tx;
static uint8_t rx_data[QUEUE_MAX][RX_BYTES], tx_data[QUEUE_MAX][4100];
static uint16_t free_tx[QUEUE_MAX], free_count;
static uint16_t base;

static uint32_t in32(uint16_t port)
{
    uint32_t value;
    __asm__ volatile("inl %1,%0" : "=a"(value) : "Nd"(port));
    return value;
}

static void out32(uint16_t port, uint32_t value)
{
    __asm__ volatile("outl %0,%1" :: "a"(value), "Nd"(port));
}

static void barrier(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

static void enqueue(struct queue *q, uint16_t id)
{
    uint16_t i = q->avail->index;
    q->avail->ring[i % q->count] = id;
    barrier();
    q->avail->index = (uint16_t)(i + 1);
}

static int setup(struct queue *q, int number)
{
    outw(base + 14, (uint16_t)number);
    q->count = inw(base + 12);
    if (!q->count || q->count > QUEUE_MAX) return 0;
    q->desc = (void *)q->pages;
    q->avail = (void *)(q->pages + 16 * q->count);
    uintptr_t end = (uintptr_t)q->avail + 6 + 2 * q->count;
    q->used = (void *)((end + PAGE - 1) & ~(uintptr_t)(PAGE - 1));
    q->avail->flags = 1;
    out32(base + 8, (uint32_t)((uintptr_t)q->pages >> 12));
    return 1;
}

int desktop_link_init(void)
{
    for (unsigned dev = 0; dev < 32; dev++) {
        uint32_t addr = 0x80000000u | dev << 11;
        out32(0xcf8, addr);
        if (in32(0xcfc) != 0x10031af4u) continue;
        out32(0xcf8, addr + 16);
        uint32_t bar = in32(0xcfc);
        if (!(bar & 1) || bar >= 65536 || !(bar & ~3u)) continue;
        base = (uint16_t)(bar & ~3u);
        out32(0xcf8, addr + 4);
        outw(0xcfc, inw(0xcfc) | 5);
        outb(base + 18, 0);
        outb(base + 18, 3);
        out32(base + 4, 0);
        if (!setup(&rx, 0) || !setup(&tx, 1)) {
            outb(base + 18, 128);
            base = 0;
            return 0;
        }
        for (uint16_t i = 0; i < rx.count; i++) {
            rx.desc[i] = (struct descriptor){(uintptr_t)rx_data[i], RX_BYTES, 2, 0};
            enqueue(&rx, i);
        }
        for (uint16_t i = 0; i < tx.count; i++) free_tx[free_count++] = i;
        barrier();
        outb(base + 18, 7);
        outw(base + 16, 0);
        return 1;
    }
    return 0;
}

int desktop_link_send(char type, unsigned slot, const void *data, unsigned length)
{
    if (!base || length > sizeof(tx_data[0]) - 4 || slot > 255) return 0;
    while (tx.taken != tx.used->index) {
        barrier();
        uint32_t id = tx.used->ring[tx.taken++ % tx.count].id;
        if (id < tx.count && free_count < tx.count) free_tx[free_count++] = (uint16_t)id;
    }
    if (!free_count) return 0;
    uint16_t id = free_tx[--free_count];
    uint8_t *p = tx_data[id];
    p[0] = (uint8_t)type; p[1] = (uint8_t)slot; p[2] = (uint8_t)length; p[3] = (uint8_t)(length >> 8);
    if (length) memcpy(p + 4, data, length);
    tx.desc[id] = (struct descriptor){(uintptr_t)p, length + 4, 0, 0};
    enqueue(&tx, id);
    barrier();
    outw(base + 16, 1);
    return 1;
}

static uint8_t packet[4 + UINT16_MAX];
static unsigned have;
static void parse(const uint8_t *bytes, unsigned n,
                  void (*message)(char, unsigned, const uint8_t *, unsigned))
{
    while (n) {
        unsigned end = have < 4 ? 4 : 4u + packet[2] + ((unsigned)packet[3] << 8);
        unsigned take = n < end - have ? n : end - have;
        memcpy(packet + have, bytes, take);
        bytes += take; have += take; n -= take;
        if (have >= 4 && have == 4u + packet[2] + ((unsigned)packet[3] << 8)) {
            message((char)packet[0], packet[1], packet + 4, have - 4);
            have = 0;
        }
    }
}

void desktop_link_poll(void (*message)(char, unsigned, const uint8_t *, unsigned))
{
    if (!base || rx.taken == rx.used->index) return;
    for (unsigned budget = 0; budget < rx.count && rx.taken != rx.used->index; budget++) {
        barrier();
        unsigned i = rx.taken++ % rx.count;
        uint32_t id = rx.used->ring[i].id, len = rx.used->ring[i].size;
        if (id >= rx.count) continue;
        if (len <= RX_BYTES) parse(rx_data[id], len, message);
        enqueue(&rx, (uint16_t)id);
    }
    barrier();
    outw(base + 16, 0);
}

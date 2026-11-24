/* Native USB root-port probe. Uses one temporary slot at a time and releases
 * the controller before reusing DMA storage on another PCI function. */
#include <wifi.h>
#include <rtl8822bu.h>
#include <interrupt/pit.h>
#include <interrupt/ports.h>
#include <log/klog.h>
#include <metta/vmm.h>
#include <string.h>

#define ALIGN(n) __attribute__((aligned(n)))
#define RING_SIZE 64
#define TYPE(n) ((uint32_t)(n) << 10)
#define REG(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
struct trb { uint64_t pointer; uint32_t size, flags; };
struct ring { struct trb trbs[RING_SIZE]; unsigned next, cycle; } ALIGN(4096);
static struct ring command, control;
static volatile struct trb events[RING_SIZE] ALIGN(4096);
static struct { uint64_t address; uint32_t count, reserved; } segment ALIGN(64);
static uint64_t dcbaa[256] ALIGN(4096), scratch_ptr[32] ALIGN(4096);
static uint8_t scratch[32][4096] ALIGN(4096);
static uint32_t input[33*16] ALIGN(4096), output[32*16] ALIGN(4096);
static uint8_t data[256] ALIGN(64);
static uintptr_t op, ir, door;
static unsigned event_index, event_cycle, stride, slot;
static int wedged;
static const char *status = "Sin explorar";
const char *wifi_description(void) { return status; }

static uint32_t pci_read(uint32_t addr) {
    uint32_t value;
    __asm__ volatile("outl %0,%1" :: "a"(addr), "Nd"((uint16_t)0xcf8));
    __asm__ volatile("inl %1,%0" : "=a"(value) : "Nd"((uint16_t)0xcfc));
    return value;
}
static void pci_command(uint32_t addr, uint16_t value) {
    pci_read(addr + 4);
    outw(0xcfc, value);
}
static void reg64(uintptr_t addr, uint64_t value) {
    REG(addr) = value; REG(addr+4) = value >> 32;
}
static int wait_bits(uintptr_t addr, uint32_t mask, uint32_t value) {
    uint64_t limit = pit_ticks_get() + 110;
    do {
        if ((REG(addr) & mask) == value) return 1;
        __asm__ volatile("pause");
    } while (pit_ticks_get() < limit);
    return 0;
}
static void reset_ring(struct ring *r) {
    memset(r, 0, sizeof(*r));
    r->cycle = 1;
}
static uintptr_t push(struct ring *r, uint64_t ptr, unsigned size, unsigned flags) {
    struct trb *t = &r->trbs[r->next++];
    t->pointer = ptr; t->size = size;
    __asm__ volatile("sfence" ::: "memory");
    t->flags = flags | r->cycle;
    if (r->next == RING_SIZE-1) {
        struct trb *link = &r->trbs[r->next];
        link->pointer = (uintptr_t)r->trbs;
        __asm__ volatile("sfence" ::: "memory");
        link->flags = TYPE(6) | 2 | r->cycle;
        r->cycle ^= 1; r->next = 0;
    }
    return (uintptr_t)t;
}
static int complete(unsigned type, uintptr_t ptr, unsigned *id) {
    uint64_t limit = pit_ticks_get() + 110;
    do {
        volatile struct trb *t = &events[event_index];
        uint32_t flags = t->flags;
        if ((flags & 1) != event_cycle) continue;
        __asm__ volatile("lfence" ::: "memory");
        uint64_t source = t->pointer;
        unsigned code = t->size >> 24;
        if (++event_index == RING_SIZE) { event_index = 0; event_cycle ^= 1; }
        reg64(ir+0x18, (uintptr_t)&events[event_index] | 8);
        if (((flags >> 10) & 63) != type || source != ptr) continue;
        if (id) *id = flags >> 24;
        return code == 1;
    } while (pit_ticks_get() < limit);
    return 0;
}
static int issue(unsigned type, unsigned device, uintptr_t ctx, unsigned *id) {
    uintptr_t t = push(&command, ctx, 0, TYPE(type) | (device << 24));
    REG(door) = 0;
    return complete(33, t, id);
}
static int transfer(uint8_t kind, uint8_t request, uint16_t value, unsigned len) {
    if (len > sizeof(data)) return 0;
    uint64_t setup = kind | ((uint64_t)request << 8) | ((uint64_t)value << 16) |
                     ((uint64_t)len << 48);
    push(&control, setup, 8, TYPE(2) | (1<<6) | (len ? 3u<<16 : 0));
    if (len) push(&control, (uintptr_t)data, len, TYPE(3) | (1<<16));
    uintptr_t t = push(&control, 0, 0, TYPE(4) | (1<<5) | (len ? 0 : 1<<16));
    REG(door + 4*slot) = 1;
    return complete(32, t, 0);
}
static int probe_port(unsigned port) {
    uintptr_t p = op+0x400+16*(port-1);
    uint32_t ps = REG(p);
    if (!(ps & 1)) return 0;
    if (!(ps & 2)) {
        /* Preserve port power; avoid writing change bits or PED. */
        REG(p) = (ps & (1<<9)) | (1<<4);
        if (!wait_bits(p, 0x13, 3)) return 0;
    }
    unsigned speed = (REG(p) >> 10) & 15;
    if (!issue(9, 0, 0, &slot) || !slot) return -1;
    reset_ring(&control);
    memset(input, 0, sizeof(input)); memset(output, 0, sizeof(output));
    dcbaa[slot] = (uintptr_t)output;
    input[1] = 3;
    uint32_t *device = input+stride, *ep = device+stride;
    device[0] = (1u<<27) | (speed<<20); device[1] = port<<16;
    unsigned packet = speed >= 4 ? 512 : speed == 3 ? 64 : 8;
    ep[1] = (packet<<16) | (4<<3) | (3<<1);
    ep[2] = (uintptr_t)control.trbs | 1; ep[4] = 8;
    int found = 0;
    if (issue(11, slot, (uintptr_t)input, 0) && transfer(0x80, 6, 0x0100, 18) &&
        data[0] == 18 && data[1] == 1) {
        unsigned vid = data[8] | (data[9]<<8), pid = data[10] | (data[11]<<8);
        KLOG_INFO("[usb] root port %u device %x:%x\n", port, vid, pid);
        if (vid == 0x2357 && pid == 0x0138) {
            status = rtl8822bu_probe(transfer, data, sizeof(data));
            found = 1;
        }
    }
    if (!issue(10, slot, 0, 0)) return -1;
    dcbaa[slot] = 0;
    return found;
}
static int probe_controller(uint32_t pci) {
    uint32_t bar = pci_read(pci+16);
    if ((bar & 1) || ((bar & 6) == 4 && pci_read(pci+20))) return -1;
    uintptr_t cap = bar & ~15u;
    if (!cap || cap > 0xfffe0000u) return -1;
    uint16_t saved = pci_read(pci+4);
    pci_command(pci, saved | 2);
    if (vmm_map_identity_range(cap, 0x10000) != METTA_OK) goto fail;
    unsigned caplen = REG(cap) & 255, hcs = REG(cap+4), hcs2 = REG(cap+8);
    uint32_t hcc = REG(cap+16);
    unsigned pads = ((hcs2>>27)&31) | (((hcs2>>21)&31)<<5);
    uint32_t dboff = REG(cap+20)&~3u, rtsoff = REG(cap+24)&~31u;
    if (caplen < 32 || !(hcs & 255) || pads > 32 || dboff > 0xfbfc || rtsoff > 0xffa0) goto fail;
    unsigned ext = (hcc>>16)*4;
    for (unsigned n=0; ext && n<256; n++) {
        if (ext > 0xfff8) goto fail;
        uint32_t e = REG(cap+ext);
        if ((e & 255) == 1) {
            REG(cap+ext) = e | (1<<24);
            if (!wait_bits(cap+ext, 1<<16, 0)) goto fail;
            REG(cap+ext+4) = 0; /* disable firmware USB SMIs */
        }
        if (!(e & 0xff00)) break;
        ext += ((e>>8)&255)*4;
    }
    op = cap+caplen; ir = cap+rtsoff+32; door = cap+dboff;
    if (!(REG(op+8)&1) || !wait_bits(op+4, 1<<11, 0)) goto fail;
    REG(op) &= ~1u;
    if (!wait_bits(op+4, 1, 1)) goto fail;
    REG(op) = 2;
    if (!wait_bits(op, 2, 0) || !wait_bits(op+4, 1<<11, 0)) goto fail;
    stride = hcc & 4 ? 16 : 8;
    memset(dcbaa, 0, sizeof(dcbaa));
    if (pads) {
        for (unsigned i=0; i<pads; i++) scratch_ptr[i] = (uintptr_t)scratch[i];
        dcbaa[0] = (uintptr_t)scratch_ptr;
    }
    reset_ring(&command); memset((void *)events, 0, sizeof(events));
    segment.address = (uintptr_t)events; segment.count = RING_SIZE;
    event_cycle = 1; event_index = 0;
    REG(op+0x38) = 1;
    reg64(op+0x30, (uintptr_t)dcbaa); reg64(op+0x18, (uintptr_t)command.trbs|1);
    REG(ir+8) = 1; reg64(ir+0x10, (uintptr_t)&segment); reg64(ir+0x18, (uintptr_t)events);
    pci_command(pci, saved | 6);
    REG(op) = 1;
    int result = -1;
    if (wait_bits(op+4, 1, 0)) {
        result = 0;
        for (unsigned p=1; p<=hcs>>24 && !result; p++) result = probe_port(p);
    }
    REG(op) = 0;
    if (!wait_bits(op+4, 1, 1)) {
        wedged = 1; pci_command(pci, saved & ~4u); return -1;
    }
    pci_command(pci, saved);
    return result;
fail:
    pci_command(pci, saved);
    return -1;
}
const char *wifi_probe(void) {
    unsigned controllers = 0, failed = 0;
    if (!wedged) {
        for (unsigned bus=0; bus<256; bus++) for (unsigned dev=0; dev<32; dev++) {
            uint32_t addr = 0x80000000u | (bus<<16) | (dev<<11);
            if ((pci_read(addr)&65535) == 65535) continue;
            unsigned functions = pci_read(addr+12)&0x800000 ? 8 : 1;
            for (unsigned f=0; f<functions; f++) {
                uint32_t pci = addr | (f<<8);
                if ((pci_read(pci+8)>>8) != 0x0c0330) continue;
                controllers++;
                int result = probe_controller(pci);
                if (result > 0) return status;
                if (result < 0) failed++;
                if (wedged) goto done;
            }
        }
    }
done:
    if (wedged || failed) status = "Exploracion xHCI incompleta";
    else if (controllers) status = "Archer T3U Plus ausente en puertos raiz";
    else status = "Sin controlador USB xHCI";
    KLOG_INFO("[wifi] %s\n", status);
    return status;
}

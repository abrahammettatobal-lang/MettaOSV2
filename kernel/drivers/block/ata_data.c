/* Primary slave only. The first sector is a host-created volume opt-in label. */
#include "ata_data.h"
#include <interrupt/ports.h>
#define ATA 0x1f0
static int mounted;
static const char label[] = "MettaOS data volume v1";
static uint16_t sector[256];

static void ata_irq_off(void)
{
    __asm__ volatile("cli" ::: "memory");
}

static void ata_irq_on(void)
{
    __asm__ volatile("sti" ::: "memory");
}

/* nIEN: el disco no debe interrumpir a mitad de un PIO. */
static void ata_polled(void)
{
    outb(0x3f6, 0x02);
}

static int wait_status(int data)
{
    for (unsigned i = 0; i < 150000; i++) {
        uint8_t s = inb(ATA + 7);
        if (!s || s == 255)
            return 0;
        if (s & 0x80)
            continue;
        if (s & 0x21)
            return 0;
        if (!data || (s & 8))
            return 1;
    }
    return 0;
}
static void select_slave(unsigned high)
{
    outb(ATA + 6, 0xf0 | high);
    for (int i = 0; i < 4; i++)
        (void)inb(0x3f6);
}
static metta_status_t transfer(unsigned lba, void *buf, unsigned count, int writing)
{
    uint8_t *p = buf;
    select_slave(lba >> 24);
    if (!wait_status(0) || !count || count > 255)
        return METTA_ERR_INTERNAL;
    outb(ATA + 2, count);
    outb(ATA + 3, lba);
    outb(ATA + 4, lba >> 8);
    outb(ATA + 5, lba >> 16);
    outb(ATA + 7, writing ? 0x30 : 0x20);
    while (count--) {
        if (!wait_status(1))
            return METTA_ERR_INTERNAL;
        if (writing) {
            uint64_t words = 256;
            __asm__ volatile("cld; rep outsw" : "+S"(p), "+c"(words) : "d"((uint16_t)ATA) : "memory");
        } else {
            uint64_t words = 256;
            __asm__ volatile("cld; rep insw" : "+D"(p), "+c"(words) : "d"((uint16_t)ATA) : "memory");
        }
        for (int i = 0; i < 4; i++)
            (void)inb(0x3f6);
    }
    if (!wait_status(0))
        return METTA_ERR_INTERNAL;
    if (writing) {
        outb(ATA + 7, 0xe7);
        if (!wait_status(0))
            return METTA_ERR_INTERNAL;
    }
    return METTA_OK;
}
static metta_status_t transfer_guarded(unsigned lba, void *buf, unsigned count, int writing)
{
    metta_status_t st;
    int try;

    for (try = 0; try < 4; try++) {
        ata_irq_off();
        ata_polled();
        st = transfer(lba, buf, count, writing);
        ata_irq_on();
        if (st == METTA_OK) {
            return st;
        }
    }
    return METTA_ERR_INTERNAL;
}
static metta_status_t read_sectors(metta_block_dev_t *d, metta_blk_lba_t l, void *p, uint32_t n)
{
    (void)d;
    return transfer_guarded((unsigned)l + 8, p, n, 0);
}
static metta_status_t write_sectors(metta_block_dev_t *d, metta_blk_lba_t l, void *p, uint32_t n)
{
    (void)d;
    return transfer_guarded((unsigned)l + 8, p, n, 1);
}
static metta_block_dev_t disk = {"ata1", 0, read_sectors, write_sectors, 0};
metta_block_dev_t *ata_data_device(void)
{
    select_slave(0);
    outb(ATA + 2, 0);
    outb(ATA + 3, 0);
    outb(ATA + 4, 0);
    outb(ATA + 5, 0);
    outb(ATA + 7, 0xec);
    if (!wait_status(1))
        return 0;
    uint16_t *p = sector;
    unsigned words = 256;
    __asm__ volatile("rep insw" : "+D"(p), "+c"(words) : "d"((uint16_t)ATA) : "memory");
    unsigned sectors = sector[60] | ((unsigned)sector[61] << 16);
    /* Hasta 1 GiB. El volumen de Firefox ocupa unos 128 MiB. */
    if (sectors < 8192 || sectors > (1024u * 1024u * 2u) || transfer(0, sector, 1, 0) != METTA_OK)
        return 0;
    for (unsigned i = 0; i < sizeof(label); i++)
        if (((char *)sector)[i] != label[i])
            return 0;
    disk.sector_count = sectors - 8;
    return &disk;
}
int ata_data_mounted(void) { return mounted; }
void ata_set_mounted(void) { mounted = 1; }

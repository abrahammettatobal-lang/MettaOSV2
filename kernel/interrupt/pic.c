#include "interrupt/pic.h"

#include "interrupt/ports.h"

void pic_init(void)
{
    /* Remap maestro/esclavo a vectores 0x20–0x2F (contrato SA-05 / IDT). */
    (void)inb(PIC_MASTER_DATA);
    (void)inb(PIC_SLAVE_DATA);

    outb(PIC_MASTER_CMD, 0x11);
    io_wait();
    outb(PIC_SLAVE_CMD, 0x11);
    io_wait();

    outb(PIC_MASTER_DATA, PIC_IRQ_VECTOR_BASE);
    io_wait();
    outb(PIC_SLAVE_DATA, (uint8_t)(PIC_IRQ_VECTOR_BASE + 8u));
    io_wait();

    outb(PIC_MASTER_DATA, 0x04);
    io_wait();
    outb(PIC_SLAVE_DATA, 0x02);
    io_wait();

    outb(PIC_MASTER_DATA, 0x01); /* modo 8086 */
    io_wait();
    outb(PIC_SLAVE_DATA, 0x01);
    io_wait();

    outb(PIC_MASTER_DATA, 0xFF);
    io_wait();
    outb(PIC_SLAVE_DATA, 0xFF);
    io_wait();
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8u) {
        outb(PIC_SLAVE_CMD, PIC_EOI);
    }
    outb(PIC_MASTER_CMD, PIC_EOI);
}

void pic_mask_irq(uint8_t irq)
{
    uint16_t port = PIC_MASTER_DATA;
    if (irq >= 8u) {
        irq = (uint8_t)(irq - 8u);
        port = PIC_SLAVE_DATA;
    }
    uint8_t mask = (uint8_t)(inb(port) | (uint8_t)(1u << irq));
    outb(port, mask);
}

void pic_unmask_irq(uint8_t irq)
{
    uint16_t port = PIC_MASTER_DATA;
    if (irq >= 8u) {
        irq = (uint8_t)(irq - 8u);
        port = PIC_SLAVE_DATA;
    }
    uint8_t mask = (uint8_t)(inb(port) & (uint8_t) ~(1u << irq));
    outb(port, mask);
}
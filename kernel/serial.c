#include "serial.h"

#include <stdint.h>

#include "interrupt/ports.h"

#define COM1 (SERIAL_COM1_BASE)

enum serial_reg {
    SERIAL_DATA       = 0,
    SERIAL_IER        = 1,
    SERIAL_FCR_IIR    = 2,
    SERIAL_LCR        = 3,
    SERIAL_MCR        = 4,
    SERIAL_LSR        = 5,
};

#define LSR_DATA_READY 0x01u
#define LSR_TX_EMPTY   0x20u

static int serial_tx_ready(void)
{
    return (inb(COM1 + SERIAL_LSR) & LSR_TX_EMPTY) != 0;
}

static void serial_wait_tx(void)
{
    while (!serial_tx_ready()) {
    }
}

void serial_init(void)
{
    outb(COM1 + SERIAL_IER, 0x00);
    outb(COM1 + SERIAL_LCR, 0x80);
    /* 115200 baud: divisor 1 */
    outb(COM1 + SERIAL_DATA, 0x01);
    outb(COM1 + SERIAL_IER, 0x00);
    outb(COM1 + SERIAL_LCR, 0x03); /* 8N1 */
    outb(COM1 + SERIAL_FCR_IIR, 0xC7);
    outb(COM1 + SERIAL_MCR, 0x0B);
    outb(COM1 + SERIAL_IER, 0x00); /* sin IRQ UART hasta driver dedicado */
}

void serial_putc(char c)
{
    serial_wait_tx();
    outb(COM1 + SERIAL_DATA, (uint8_t)c);
}

void serial_write(const char *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '\n') {
            serial_putc('\r');
        }
        serial_putc(c);
    }
}

void serial_write_str(const char *s)
{
    if (s == NULL) {
        return;
    }
    while (*s != '\0') {
        char c = *s++;
        if (c == '\n') {
            serial_putc('\r');
        }
        serial_putc(c);
    }
}
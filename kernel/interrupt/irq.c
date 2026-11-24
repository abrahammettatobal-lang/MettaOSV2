#include "interrupt/irq.h"

#include "interrupt/pic.h"
#include "interrupt/pit.h"

#define IRQ_COUNT 16u

static irq_handler_t irq_handlers[IRQ_COUNT];

void irq_init(void)
{
    for (uint8_t i = 0; i < IRQ_COUNT; i++) {
        irq_handlers[i] = NULL;
    }
    irq_register_handler(IRQ_TIMER, pit_on_irq);
}

void irq_register_handler(uint8_t irq, irq_handler_t handler)
{
    if (irq >= IRQ_COUNT) {
        return;
    }
    irq_handlers[irq] = handler;
}

void irq_dispatch(uint8_t irq)
{
    if (irq < IRQ_COUNT && irq_handlers[irq] != NULL) {
        irq_handlers[irq]();
    }
    pic_send_eoi(irq);
}

void irq0_entry(void)
{
    irq_dispatch(IRQ_TIMER);
}
#ifndef METTA_INTERRUPTS_IRQ_H
#define METTA_INTERRUPTS_IRQ_H

#include <stdint.h>

#define IRQ_TIMER 0u

typedef void (*irq_handler_t)(void);

void irq_init(void);
void irq_register_handler(uint8_t irq, irq_handler_t handler);
void irq_dispatch(uint8_t irq);

/* Invocable desde el stub de ensamblador del vector IDT 0x20 (IRQ0). */
void irq0_entry(void);

#endif /* METTA_INTERRUPTS_IRQ_H */
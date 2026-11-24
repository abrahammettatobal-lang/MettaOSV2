#ifndef METTA_INTERRUPTS_PIT_H
#define METTA_INTERRUPTS_PIT_H

#include <stdint.h>

#define PIT_BASE_HZ     1193182u
#define PIT_TICK_HZ     100u
#define PIT_PORT_DATA   0x40u
#define PIT_PORT_CMD    0x43u

void pit_init(void);
void pit_on_irq(void);
uint64_t pit_ticks_get(void);
void pit_sleep_ms(uint32_t ms);

#endif /* METTA_INTERRUPTS_PIT_H */
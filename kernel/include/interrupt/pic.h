#ifndef METTA_INTERRUPTS_PIC_H
#define METTA_INTERRUPTS_PIC_H

#include <stdint.h>

#define PIC_MASTER_CMD  0x20u
#define PIC_MASTER_DATA 0x21u
#define PIC_SLAVE_CMD   0xA0u
#define PIC_SLAVE_DATA  0xA1u

#define PIC_IRQ_VECTOR_BASE 0x20u

#define PIC_EOI 0x20u

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

#endif /* METTA_INTERRUPTS_PIC_H */
#ifndef METTA_CPU_IDT_H
#define METTA_CPU_IDT_H

#include <stdint.h>

#define IDT_VECTOR_COUNT 256u

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_pointer_t;

void idt_init(void);
void idt_set_gate(uint8_t vector, void (*handler)(void), uint8_t type_attr, uint8_t ist);
const idt_pointer_t *idt_pointer_get(void);

#endif /* METTA_CPU_IDT_H */
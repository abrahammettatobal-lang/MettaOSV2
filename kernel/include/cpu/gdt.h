#ifndef METTA_CPU_GDT_H
#define METTA_CPU_GDT_H

#include <stdint.h>

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_pointer_t;

void gdt_init(void);
void gdt_install_tss_descriptor(void);
const gdt_pointer_t *gdt_pointer_get(void);

#endif /* METTA_CPU_GDT_H */
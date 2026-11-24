#ifndef METTA_CPU_CPU_H
#define METTA_CPU_CPU_H

/*
 * Contrato SA-05 / boot-map: invocar desde kmain tras VMM y antes de PIC/IRQ/sti.
 */
void cpu_init(void);

/* Alias documentado en boot-map.md */
void arch_init_idt_gdt_tss(void);

#endif /* METTA_CPU_CPU_H */
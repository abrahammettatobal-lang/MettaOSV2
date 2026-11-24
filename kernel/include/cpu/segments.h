#ifndef METTA_CPU_SEGMENTS_H
#define METTA_CPU_SEGMENTS_H

/* Selectores de segmento (índice GDT << 3 | RPL). */
#define GDT_SEL_NULL   0x00u
#define GDT_SEL_KCODE  0x08u
#define GDT_SEL_KDATA  0x10u
/* SYSRET: SS = STAR[63:48]+8, CS = STAR[63:48]+16. STAR usuario = 0x10. */
#define GDT_SEL_UDATA  0x1Bu /* índice 3, ring 3 */
#define GDT_SEL_UCODE  0x23u /* índice 4, ring 3 */
#define GDT_SEL_TSS    0x28u

#define GDT_ENTRY_COUNT 8u

/* Atributos para gates IDT (presente, DPL 0, interrupt gate 64-bit). */
#define IDT_ATTR_KERNEL_GATE  0x8Eu
#define IDT_ATTR_USER_GATE    0xEEu /* futuro syscall/int 0x80 */

#endif /* METTA_CPU_SEGMENTS_H */
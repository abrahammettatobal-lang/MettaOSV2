#ifndef METTA_CPU_TSS_H
#define METTA_CPU_TSS_H

#include <stdint.h>

/*
 * TSS 64-bit mínima (Intel SDM). RSP0 para entradas ring 0 desde ring 3;
 * IST1 reservado para #DF. RSP1/RSP2 sin usar en bring-up uniprocessor.
 */
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss64_t;

void tss_prepare(void);
void tss_load_task_register(void);
tss64_t *tss_get(void);

/* Stub Ring 3: selectores listos en GDT; RSP usuario no fijado hasta VMM/sched. */
uint16_t tss_user_code_selector(void);
uint16_t tss_user_data_selector(void);

#endif /* METTA_CPU_TSS_H */
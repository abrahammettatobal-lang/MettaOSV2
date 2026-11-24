#include <stddef.h>

#include "cpu/tss.h"
#include "cpu/segments.h"

#define TSS_IST0_STACK_SIZE 16384u
#define TSS_IST1_STACK_SIZE 4096u

static tss64_t tss;

static uint8_t ist0_stack[TSS_IST0_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t ist1_stack[TSS_IST1_STACK_SIZE] __attribute__((aligned(16)));

static uintptr_t stack_top(const uint8_t *base, size_t size)
{
    return (uintptr_t)(base + size);
}

void tss_prepare(void)
{
    tss = (tss64_t){0};

    tss.rsp0 = stack_top(ist0_stack, TSS_IST0_STACK_SIZE);
    tss.ist1 = stack_top(ist1_stack, TSS_IST1_STACK_SIZE);
    tss.iopb_offset = (uint16_t)sizeof(tss64_t);
}

void tss_load_task_register(void)
{
    __asm__ volatile("ltr %w0" : : "r"(GDT_SEL_TSS) : "memory");
}

tss64_t *tss_get(void)
{
    return &tss;
}

uint16_t tss_user_code_selector(void)
{
    return GDT_SEL_UCODE;
}

uint16_t tss_user_data_selector(void)
{
    return GDT_SEL_UDATA;
}
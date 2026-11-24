#include "cpu/idt.h"
#include "cpu/segments.h"

#include "interrupt/irq.h"
#include "interrupt/pic.h"

static idt_entry_t idt[IDT_VECTOR_COUNT];
static idt_pointer_t idt_ptr;

extern void isr_stub_0(void);
extern void isr_stub_1(void);
extern void isr_stub_2(void);
extern void isr_stub_3(void);
extern void isr_stub_4(void);
extern void isr_stub_5(void);
extern void isr_stub_6(void);
extern void isr_stub_7(void);
extern void isr_stub_8(void);
extern void isr_stub_9(void);
extern void isr_stub_10(void);
extern void isr_stub_11(void);
extern void isr_stub_12(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);
extern void isr_stub_15(void);
extern void isr_stub_16(void);
extern void isr_stub_17(void);
extern void isr_stub_18(void);
extern void isr_stub_19(void);
extern void isr_stub_20(void);
extern void isr_stub_21(void);
extern void isr_stub_22(void);
extern void isr_stub_23(void);
extern void isr_stub_24(void);
extern void isr_stub_25(void);
extern void isr_stub_26(void);
extern void isr_stub_27(void);
extern void isr_stub_28(void);
extern void isr_stub_29(void);
extern void isr_stub_30(void);
extern void isr_stub_31(void);
extern void isr_stub_32(void);
extern void isr_stub_33(void);
extern void isr_stub_34(void);
extern void isr_stub_35(void);
extern void isr_stub_36(void);
extern void isr_stub_37(void);
extern void isr_stub_38(void);
extern void isr_stub_39(void);
extern void isr_stub_40(void);
extern void isr_stub_41(void);
extern void isr_stub_42(void);
extern void isr_stub_43(void);
extern void isr_stub_44(void);
extern void isr_stub_45(void);
extern void isr_stub_46(void);
extern void isr_stub_47(void);

static void (*const exception_stubs[32])(void) = {
    isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3,
    isr_stub_4,  isr_stub_5,  isr_stub_6,  isr_stub_7,
    isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11,
    isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15,
    isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19,
    isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23,
    isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27,
    isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31,
};

static void (*const irq_stubs[16])(void) = {
    isr_stub_32, isr_stub_33, isr_stub_34, isr_stub_35,
    isr_stub_36, isr_stub_37, isr_stub_38, isr_stub_39,
    isr_stub_40, isr_stub_41, isr_stub_42, isr_stub_43,
    isr_stub_44, isr_stub_45, isr_stub_46, isr_stub_47,
};

void cpu_hwirq_entry(uint64_t vector)
{
    if (vector == 32u) {
        irq0_entry();
        return;
    }
    if (vector >= 32u && vector < 48u) {
        pic_send_eoi((uint8_t)(vector - 32u));
    }
}

void idt_set_gate(uint8_t vector, void (*handler)(void), uint8_t type_attr, uint8_t ist)
{
    uintptr_t addr = (uintptr_t)handler;

    idt[vector].offset_low = (uint16_t)(addr & 0xFFFFu);
    idt[vector].selector = GDT_SEL_KCODE;
    idt[vector].ist = ist;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_mid = (uint16_t)((addr >> 16) & 0xFFFFu);
    idt[vector].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFFu);
    idt[vector].reserved = 0;
}

static void idt_load(void)
{
    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1u);
    idt_ptr.base = (uint64_t)(uintptr_t)idt;
    __asm__ volatile("lidt %0" : : "m"(idt_ptr) : "memory");
}

void idt_init(void)
{
    for (uint16_t i = 0; i < IDT_VECTOR_COUNT; i++) {
        idt[i] = (idt_entry_t){0};
    }

    for (uint8_t v = 0; v < 32u; v++) {
        uint8_t ist = (v == 8u) ? 1u : 0u;
        idt_set_gate(v, exception_stubs[v], IDT_ATTR_KERNEL_GATE, ist);
    }

    for (uint8_t irq = 0; irq < 16u; irq++) {
        idt_set_gate((uint8_t)(32u + irq), irq_stubs[irq], IDT_ATTR_KERNEL_GATE, 0u);
    }

    idt_load();
}

const idt_pointer_t *idt_pointer_get(void)
{
    return &idt_ptr;
}
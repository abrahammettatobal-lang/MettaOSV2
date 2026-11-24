#include "cpu/cpu.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/tss.h"

#include <stdint.h>

static void cpu_enable_sse(void)
{
    uint64_t cr0;
    uint64_t cr4;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2); /* EM = 0 */
    cr0 |= (1ULL << 1);  /* MP = 1 */
    __asm__ volatile("mov %0, %%cr0" ::"r"(cr0) : "memory");

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);  /* OSFXSR */
    cr4 |= (1ULL << 10); /* OSXMMEXCPT */
    __asm__ volatile("mov %0, %%cr4" ::"r"(cr4) : "memory");
}

void cpu_init(void)
{
    /* IDT antes de LTR: un #GP en el TSS no debe triple-faultear a ciegas. */
    gdt_init();
    idt_init();
    tss_prepare();
    gdt_install_tss_descriptor();
    tss_load_task_register();
    cpu_enable_sse();
}

void arch_init_idt_gdt_tss(void)
{
    cpu_init();
}
#include "cpu/exceptions.h"

void cpu_hwirq_entry(uint64_t vector);

__attribute__((naked)) void exception_common(void)
{
    __asm__ volatile(
        "pushq %%rax\n"
        "pushq %%rcx\n"
        "pushq %%rdx\n"
        "pushq %%rbx\n"
        "pushq $0\n"
        "pushq %%rbp\n"
        "pushq %%rsi\n"
        "pushq %%rdi\n"
        "pushq %%r8\n"
        "pushq %%r9\n"
        "pushq %%r10\n"
        "pushq %%r11\n"
        "pushq %%r12\n"
        "pushq %%r13\n"
        "pushq %%r14\n"
        "pushq %%r15\n"
        "movq %%rsp, %%rdi\n"
        "call exception_handler\n"
        "popq %%r15\n"
        "popq %%r14\n"
        "popq %%r13\n"
        "popq %%r12\n"
        "popq %%r11\n"
        "popq %%r10\n"
        "popq %%r9\n"
        "popq %%r8\n"
        "popq %%rdi\n"
        "popq %%rsi\n"
        "popq %%rbp\n"
        "addq $8, %%rsp\n"
        "popq %%rbx\n"
        "popq %%rdx\n"
        "popq %%rcx\n"
        "popq %%rax\n"
        "addq $16, %%rsp\n"
        "iretq\n"
        ::: "memory");
}

__attribute__((naked)) void irq_common(void)
{
    __asm__ volatile(
        "pushq %%rax\n"
        "pushq %%rcx\n"
        "pushq %%rdx\n"
        "pushq %%rbx\n"
        "pushq $0\n"
        "pushq %%rbp\n"
        "pushq %%rsi\n"
        "pushq %%rdi\n"
        "pushq %%r8\n"
        "pushq %%r9\n"
        "pushq %%r10\n"
        "pushq %%r11\n"
        "pushq %%r12\n"
        "pushq %%r13\n"
        "pushq %%r14\n"
        "pushq %%r15\n"
        "movq 128(%%rsp), %%rdi\n"
        "call cpu_hwirq_entry\n"
        "popq %%r15\n"
        "popq %%r14\n"
        "popq %%r13\n"
        "popq %%r12\n"
        "popq %%r11\n"
        "popq %%r10\n"
        "popq %%r9\n"
        "popq %%r8\n"
        "popq %%rdi\n"
        "popq %%rsi\n"
        "popq %%rbp\n"
        "addq $8, %%rsp\n"
        "popq %%rbx\n"
        "popq %%rdx\n"
        "popq %%rcx\n"
        "popq %%rax\n"
        "addq $16, %%rsp\n"
        "iretq\n"
        ::: "memory");
}

#define EXC_STUB(num, push_err)                                    \
    __attribute__((naked)) void isr_stub_##num(void)              \
    {                                                              \
        __asm__ volatile(push_err "pushq $" #num "\n"               \
                                 "jmp exception_common\n");        \
    }

EXC_STUB(0, "pushq $0\n")
EXC_STUB(1, "pushq $0\n")
EXC_STUB(2, "pushq $0\n")
EXC_STUB(3, "pushq $0\n")
EXC_STUB(4, "pushq $0\n")
EXC_STUB(5, "pushq $0\n")
EXC_STUB(6, "pushq $0\n")
EXC_STUB(7, "pushq $0\n")
EXC_STUB(8, "")
EXC_STUB(9, "pushq $0\n")
EXC_STUB(10, "")
EXC_STUB(11, "")
EXC_STUB(12, "")
EXC_STUB(13, "")
EXC_STUB(14, "")
EXC_STUB(15, "pushq $0\n")
EXC_STUB(16, "pushq $0\n")
EXC_STUB(17, "")
EXC_STUB(18, "pushq $0\n")
EXC_STUB(19, "pushq $0\n")
EXC_STUB(20, "pushq $0\n")
EXC_STUB(21, "")

EXC_STUB(22, "pushq $0\n")
EXC_STUB(23, "pushq $0\n")
EXC_STUB(24, "pushq $0\n")
EXC_STUB(25, "pushq $0\n")
EXC_STUB(26, "pushq $0\n")
EXC_STUB(27, "pushq $0\n")
EXC_STUB(28, "pushq $0\n")
EXC_STUB(29, "pushq $0\n")
EXC_STUB(30, "pushq $0\n")
EXC_STUB(31, "pushq $0\n")

#define IRQ_STUB(num)                                              \
    __attribute__((naked)) void isr_stub_##num(void)              \
    {                                                              \
        __asm__ volatile("pushq $0\n"                               \
                                 "pushq $" #num "\n"               \
                                 "jmp irq_common\n");              \
    }

IRQ_STUB(32)
IRQ_STUB(33)
IRQ_STUB(34)
IRQ_STUB(35)
IRQ_STUB(36)
IRQ_STUB(37)
IRQ_STUB(38)
IRQ_STUB(39)
IRQ_STUB(40)
IRQ_STUB(41)
IRQ_STUB(42)
IRQ_STUB(43)
IRQ_STUB(44)
IRQ_STUB(45)
IRQ_STUB(46)
IRQ_STUB(47)
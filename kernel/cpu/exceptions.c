#include "cpu/exceptions.h"

#include "kernel.h"
#include "log/printk.h"

#include <linux/usermode.h>

#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n);

static const char *exception_name(uint64_t vector)
{
    static const char *const names[] = {
        "#DE divide",        "#DB debug",           "NMI",              "#BP breakpoint",
        "#OF overflow",      "#BR bound",           "#UD invalid op",   "#NM device",
        "#DF double fault",  "#CP coproc seg",      "#TS invalid TSS",  "#NP segment",
        "#SS stack",         "#GP general",         "#PF page fault",   "reserved",
        "#MF x87",           "#AC alignment",       "#MC machine",      "#XM SIMD",
        "#VE virtualization"
    };

    if (vector < (sizeof(names) / sizeof(names[0]))) {
        return names[vector];
    }
    return "unknown";
}

void exception_handler(cpu_exception_frame_t *frame)
{
    const char *name = exception_name(frame->vector);

    printk("CPU exception vector=%llu (%s) err=%llu RIP=%llx CS=%llx RFLAGS=%llx\n",
           (unsigned long long)frame->vector,
           name,
           (unsigned long long)frame->error_code,
           (unsigned long long)frame->rip,
           (unsigned long long)frame->cs,
           (unsigned long long)frame->rflags);

    if (frame->vector == 14u) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        printk("  page fault CR2=%llx rax=%llx rbx=%llx r8=%llx r12=%llx\n",
               (unsigned long long)cr2,
               (unsigned long long)frame->rax,
               (unsigned long long)frame->rbx,
               (unsigned long long)frame->r8,
               (unsigned long long)frame->r12);
        if (frame->rip >= 0x400000ull && frame->rip < 0x70000000ull) {
            const uint8_t *ip = (const uint8_t *)(uintptr_t)frame->rip;

            printk("  insn %x %x %x %x %x %x %x %x\n",
                   (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3],
                   (unsigned)ip[4], (unsigned)ip[5], (unsigned)ip[6], (unsigned)ip[7]);
            printk("  rcx=%llx rdx=%llx rsi=%llx rdi=%llx\n",
                   (unsigned long long)frame->rcx,
                   (unsigned long long)frame->rdx,
                   (unsigned long long)frame->rsi,
                   (unsigned long long)frame->rdi);
        }
        if (frame->rbx >= 0x400000ull && frame->rbx < 0x20000000ull) {
            const uint8_t *lm = (const uint8_t *)(uintptr_t)frame->rbx;
            uint64_t laddr;
            uint64_t lphdr;
            uint16_t phnum;
            memcpy(&laddr, lm, sizeof(laddr));
            memcpy(&lphdr, lm + 0x2e0, sizeof(lphdr));
            memcpy(&phnum, lm + 0x2f0, sizeof(phnum));
            printk("  link l_addr=%llx l_phdr=%llx phnum=%u\n",
                   (unsigned long long)laddr,
                   (unsigned long long)lphdr,
                   (unsigned)phnum);
        }
    }

    /* Ring 3: el proceso muere y el escritorio sigue. El kernel no se detiene. */
    if ((frame->cs & 3u) == 3u && linux_user_in_process()) {
        printk("  user fault: proceso terminado\n");
        linux_user_fault_leave();
    }

    kernel_halt();
}
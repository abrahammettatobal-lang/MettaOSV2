/*
 * MettaOS — ring 3: syscall/sysret, carga de ELF ET_EXEC/ET_DYN y retorno por exit.
 */
#include <linux/usermode.h>

#include "elf_types.h"
#include "syscall_linux.h"

#include <log/klog.h>
#include <metta/pmm.h>
#include <metta/vmm.h>

#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);

#define USER_LO      0x00400000ull
#define USER_HI      0x20000000ull
#define USER_STACK   0x1FE00000ull

extern const uint8_t hello_elf_start[];
extern const uint8_t hello_elf_end[];

volatile uint64_t linux_user_kstack;
volatile uint64_t linux_user_syscall_rsp;
volatile uint64_t linux_user_rsp;
volatile uint64_t linux_user_nr;
volatile uint64_t linux_user_rdi;
volatile uint64_t linux_user_rsi;
volatile uint64_t linux_user_rdx;
volatile uint64_t linux_user_r10;
volatile uint64_t linux_user_r8;
volatile uint64_t linux_user_r9;
volatile uint64_t linux_user_rcx;
volatile uint64_t linux_user_r11;
volatile uint64_t linux_user_rbx;
volatile uint64_t linux_user_rbp;
volatile uint64_t linux_user_r12;
volatile uint64_t linux_user_r13;
volatile uint64_t linux_user_r14;
volatile uint64_t linux_user_r15;
volatile uint32_t linux_user_leave;
int32_t  linux_user_status;
int      linux_user_trace;

extern char __kernel_image_end[];

static uint8_t  g_syscall_stack[16384] __attribute__((aligned(16)));
static int      g_cpu_ready;

static void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);

    __asm__ volatile("wrmsr" :: "c"(msr), "a"(lo), "d"(hi) : "memory");
}

static uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;

    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

void linux_user_note_exit(int status)
{
    linux_user_status = (int32_t)status;
    linux_user_leave  = 1u;
}

int linux_user_in_process(void)
{
    return linux_user_kstack != 0u;
}

/* Vuelve al caller de linux_user_enter. El proceso en ring 3 ya no existe. */
__attribute__((naked, noreturn)) void linux_user_fault_leave(void)
{
    __asm__ volatile(
        "movl $-14, linux_user_status(%%rip)\n"
        "movl $1, linux_user_leave(%%rip)\n"
        "mov linux_user_kstack(%%rip), %%rax\n"
        "movq $0, linux_user_kstack(%%rip)\n"
        "mov %%rax, %%rsp\n"
        "pop %%r15\n"
        "pop %%r14\n"
        "pop %%r13\n"
        "pop %%r12\n"
        "pop %%rbp\n"
        "pop %%rbx\n"
        "popfq\n"
        "ret\n"
        ::: "memory");
}

uint64_t linux_syscall_from_user(void)
{
    int64_t ret;

    ret = linux_syscall_dispatch_raw(linux_user_nr, linux_user_rdi, linux_user_rsi,
                                     linux_user_rdx, linux_user_r10, linux_user_r8,
                                     linux_user_r9);
    if (linux_user_trace) {
        linux_user_trace_result(linux_user_nr, linux_user_rdi, ret);
    }
    return (uint64_t)ret;
}

__attribute__((naked)) void linux_syscall_entry(void)
{
    __asm__ volatile(
        "mov %%rsp, linux_user_rsp(%%rip)\n"
        "mov %%rax, linux_user_nr(%%rip)\n"
        "mov %%rdi, linux_user_rdi(%%rip)\n"
        "mov %%rsi, linux_user_rsi(%%rip)\n"
        "mov %%rdx, linux_user_rdx(%%rip)\n"
        "mov %%r10, linux_user_r10(%%rip)\n"
        "mov %%r8, linux_user_r8(%%rip)\n"
        "mov %%r9, linux_user_r9(%%rip)\n"
        "mov %%rcx, linux_user_rcx(%%rip)\n"
        "mov %%r11, linux_user_r11(%%rip)\n"
        "mov %%rbx, linux_user_rbx(%%rip)\n"
        "mov %%rbp, linux_user_rbp(%%rip)\n"
        "mov %%r12, linux_user_r12(%%rip)\n"
        "mov %%r13, linux_user_r13(%%rip)\n"
        "mov %%r14, linux_user_r14(%%rip)\n"
        "mov %%r15, linux_user_r15(%%rip)\n"
        "mov linux_user_syscall_rsp(%%rip), %%rsp\n"
        "call linux_syscall_from_user\n"
        "mov linux_user_rsp(%%rip), %%rsp\n"
        "mov linux_user_rdi(%%rip), %%rdi\n"
        "mov linux_user_rsi(%%rip), %%rsi\n"
        "mov linux_user_rdx(%%rip), %%rdx\n"
        "mov linux_user_r10(%%rip), %%r10\n"
        "mov linux_user_r8(%%rip), %%r8\n"
        "mov linux_user_r9(%%rip), %%r9\n"
        "mov linux_user_rcx(%%rip), %%rcx\n"
        "mov linux_user_r11(%%rip), %%r11\n"
        "mov linux_user_rbx(%%rip), %%rbx\n"
        "mov linux_user_rbp(%%rip), %%rbp\n"
        "mov linux_user_r12(%%rip), %%r12\n"
        "mov linux_user_r13(%%rip), %%r13\n"
        "mov linux_user_r14(%%rip), %%r14\n"
        "mov linux_user_r15(%%rip), %%r15\n"
        "cmpl $0, linux_user_leave(%%rip)\n"
        "jne 1f\n"
        "sysretq\n"
        "1:\n"
        "mov linux_user_kstack(%%rip), %%rsp\n"
        "pop %%r15\n"
        "pop %%r14\n"
        "pop %%r13\n"
        "pop %%r12\n"
        "pop %%rbp\n"
        "pop %%rbx\n"
        "popfq\n"
        "ret\n"
        ::: "memory");
}

__attribute__((naked)) void linux_user_enter(uint64_t entry, uint64_t user_rsp)
{
    __asm__ volatile(
        "pushfq\n"
        "push %%rbx\n"
        "push %%rbp\n"
        "push %%r12\n"
        "push %%r13\n"
        "push %%r14\n"
        "push %%r15\n"
        "mov %%rsp, linux_user_kstack(%%rip)\n"
        "pushq $0x1B\n"
        "push %%rsi\n"
        "pushfq\n"
        "pop %%rax\n"
        "or $0x200, %%rax\n"
        "push %%rax\n"
        "pushq $0x23\n"
        "push %%rdi\n"
        "iretq\n"
        ::: "memory");
}

void linux_user_cpu_init(void)
{
    uint64_t efer;
    uint64_t lstar;

    if (g_cpu_ready) {
        return;
    }
    efer = rdmsr(0xC0000080u);
    wrmsr(0xC0000080u, efer | 1u);
    /* STAR: kernel CS 0x08, base de usuario 0x10 → SS 0x1B, CS 0x23 */
    wrmsr(0xC0000081u, 0x00100008ull << 32);
    lstar = (uint64_t)(uintptr_t)linux_syscall_entry;
    wrmsr(0xC0000082u, lstar);
    wrmsr(0xC0000084u, 0x47700u);
    linux_user_syscall_rsp =
        ((uint64_t)(uintptr_t)g_syscall_stack + sizeof(g_syscall_stack)) & ~0xFull;
    g_cpu_ready = 1;
}

#define CLONE_VM             0x00000100ull
#define CLONE_THREAD         0x00010000ull
#define CLONE_SETTLS         0x00080000ull
#define CLONE_PARENT_SETTID  0x00100000ull
#define CLONE_CHILD_CLEARTID 0x00200000ull
#define CLONE_CHILD_SETTID   0x01000000ull
#define CLONE_USER_HI        0x70000000ull
#define MSR_FS_BASE          0xC0000100u
#define CLONE_CHILD_TID      2

enum {
    CLONE_IDLE = 0,
    CLONE_CHILD,
    CLONE_PARKED
};

struct clone_regs {
    uint64_t rsp;
    uint64_t rcx;
    uint64_t r11;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t fs;
};

struct clone_saved {
    struct clone_regs parent;
    struct clone_regs child;
    uint64_t          clear_tid;
    uint64_t          wait_addr;
    int64_t           parent_ret;
    int               mode;
};

static struct clone_saved g_clone;

static void clone_capture(struct clone_regs *regs)
{
    regs->rsp = linux_user_rsp;
    regs->rcx = linux_user_rcx;
    regs->r11 = linux_user_r11;
    regs->rbx = linux_user_rbx;
    regs->rbp = linux_user_rbp;
    regs->r12 = linux_user_r12;
    regs->r13 = linux_user_r13;
    regs->r14 = linux_user_r14;
    regs->r15 = linux_user_r15;
    regs->rdi = linux_user_rdi;
    regs->rsi = linux_user_rsi;
    regs->rdx = linux_user_rdx;
    regs->r8  = linux_user_r8;
    regs->r9  = linux_user_r9;
    regs->r10 = linux_user_r10;
    regs->fs  = rdmsr(MSR_FS_BASE);
}

static void clone_install(const struct clone_regs *regs)
{
    linux_user_rsp = regs->rsp;
    linux_user_rcx = regs->rcx;
    linux_user_r11 = regs->r11;
    linux_user_rbx = regs->rbx;
    linux_user_rbp = regs->rbp;
    linux_user_r12 = regs->r12;
    linux_user_r13 = regs->r13;
    linux_user_r14 = regs->r14;
    linux_user_r15 = regs->r15;
    linux_user_rdi = regs->rdi;
    linux_user_rsi = regs->rsi;
    linux_user_rdx = regs->rdx;
    linux_user_r8  = regs->r8;
    linux_user_r9  = regs->r9;
    linux_user_r10 = regs->r10;
    wrmsr(MSR_FS_BASE, regs->fs);
}

static int clone_ptr_ok(uint64_t addr, uint64_t n)
{
    if (n == 0u || addr < USER_LO || addr > CLONE_USER_HI - n) {
        return 0;
    }
    return 1;
}

static void clone_store_tid(uint64_t addr, int32_t tid)
{
    if (addr != 0u && clone_ptr_ok(addr, 4u)) {
        *(volatile int32_t *)(uintptr_t)addr = tid;
    }
}

int linux_user_current_tid(void)
{
    return g_clone.mode == CLONE_CHILD ? CLONE_CHILD_TID : 1;
}

int linux_user_clone_enter(uint64_t stack, uint64_t flags, uint64_t parent_tid,
                           uint64_t child_tid, uint64_t tls)
{
    if (g_clone.mode != CLONE_IDLE) {
        return -LINUX_EAGAIN;
    }
    if ((flags & CLONE_VM) == 0u || (flags & CLONE_THREAD) == 0u || stack == 0u ||
        !clone_ptr_ok(stack, 8u)) {
        return -LINUX_EINVAL;
    }
    clone_capture(&g_clone.parent);
    g_clone.clear_tid = (flags & CLONE_CHILD_CLEARTID) != 0u ? child_tid : 0u;
    g_clone.parent_ret = CLONE_CHILD_TID;
    g_clone.wait_addr = 0;
    g_clone.mode = CLONE_CHILD;
    if ((flags & CLONE_PARENT_SETTID) != 0u) {
        clone_store_tid(parent_tid, CLONE_CHILD_TID);
    }
    if ((flags & CLONE_CHILD_SETTID) != 0u) {
        clone_store_tid(child_tid, CLONE_CHILD_TID);
    }
    if ((flags & CLONE_SETTLS) != 0u) {
        wrmsr(MSR_FS_BASE, tls);
    }
    linux_user_rsp = stack;
    KLOG_INFO("clone hijo stack=%llx flags=%llx\n",
              (unsigned long long)stack, (unsigned long long)flags);
    return 0;
}

int linux_user_clone_exit(int64_t *tid_out)
{
    if (g_clone.mode != CLONE_CHILD) {
        return 0;
    }
    if (g_clone.clear_tid != 0u) {
        clone_store_tid(g_clone.clear_tid, 0);
    }
    clone_install(&g_clone.parent);
    if (tid_out != NULL) {
        *tid_out = g_clone.parent_ret;
    }
    g_clone.mode = CLONE_IDLE;
    g_clone.clear_tid = 0;
    KLOG_INFO("clone padre reanuda\n");
    return 1;
}

int linux_user_futex_block(uint64_t uaddr, int64_t *ret_out)
{
    if (g_clone.mode != CLONE_CHILD || ret_out == NULL) {
        return 0;
    }
    clone_capture(&g_clone.child);
    g_clone.wait_addr = uaddr;
    g_clone.mode = CLONE_PARKED;
    clone_install(&g_clone.parent);
    *ret_out = g_clone.parent_ret;
    KLOG_INFO("futex hijo espera %llx\n", (unsigned long long)uaddr);
    return 1;
}

int linux_user_futex_wake(uint64_t uaddr, int64_t *ret_out)
{
    if (g_clone.mode != CLONE_PARKED || g_clone.wait_addr != uaddr || ret_out == NULL) {
        return 0;
    }
    clone_capture(&g_clone.parent);
    g_clone.parent_ret = 1;
    g_clone.mode = CLONE_CHILD;
    clone_install(&g_clone.child);
    *ret_out = 0;
    KLOG_INFO("futex despierta hijo\n");
    return 1;
}

void linux_user_clone_cancel(void)
{
    g_clone.mode = CLONE_IDLE;
    g_clone.clear_tid = 0;
}

/* Siguiente frontera de 2 MiB en o después de __kernel_image_end. */
static uint64_t kernel_image_floor(void)
{
    uint64_t end = (uint64_t)(uintptr_t)(void *)__kernel_image_end;
    const uint64_t align = 0x200000ull;

    return (end + align - 1ull) & ~(align - 1ull);
}

static uint64_t user_map_min(void)
{
    uint64_t floor = kernel_image_floor();

    return floor > USER_LO ? floor : USER_LO;
}

int linux_user_map_elf(const uint8_t *image, size_t size, uint64_t bias,
                       uint64_t *entry_out, uint64_t *phdr_out, uint64_t *phnum_out)
{
    const metta_elf64_ehdr_t *eh;
    uint64_t lim;
    uint64_t ph_bytes;
    uint64_t min_dest;
    uint64_t phdr_va;
    uint64_t entry;
    uint64_t image_end = 0;
    uint16_t i;
    int      saw_load = 0;
    int      saw_phdr = 0;

    if (image == NULL || entry_out == NULL || phdr_out == NULL || phnum_out == NULL ||
        size < sizeof(*eh)) {
        return -1;
    }
    eh = (const metta_elf64_ehdr_t *)image;
    if (eh->e_ident[EI_MAG0] != ELFMAG0 || eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 || eh->e_ident[EI_MAG3] != ELFMAG3 ||
        eh->e_ident[EI_CLASS] != ELFCLASS64 || eh->e_ident[EI_DATA] != ELFDATA2LSB ||
        eh->e_machine != EM_X86_64 ||
        (eh->e_type != ET_EXEC && eh->e_type != ET_DYN)) {
        return -1;
    }
    if (eh->e_phnum == 0 || eh->e_phentsize < sizeof(metta_elf64_phdr_t)) {
        return -1;
    }
    lim = (uint64_t)size;
    ph_bytes = (uint64_t)eh->e_phnum * (uint64_t)eh->e_phentsize;
    if (eh->e_phoff > lim || ph_bytes > lim - eh->e_phoff) {
        return -1;
    }

    min_dest = user_map_min();
    if (bias > UINT64_MAX - eh->e_phoff) {
        return -1;
    }
    phdr_va = bias + eh->e_phoff;

    for (i = 0; i < eh->e_phnum; i++) {
        const metta_elf64_phdr_t *ph =
            (const metta_elf64_phdr_t *)(image + eh->e_phoff + (uint64_t)i * eh->e_phentsize);
        uint64_t dest;
        uint8_t *dst;

        if (ph->p_type == PT_PHDR && !saw_phdr) {
            if (bias > UINT64_MAX - ph->p_vaddr) {
                return -1;
            }
            phdr_va = bias + ph->p_vaddr;
            saw_phdr = 1;
        }
        if (ph->p_type != PT_LOAD || ph->p_memsz == 0) {
            continue;
        }
        if (ph->p_vaddr > UINT64_MAX - bias) {
            return -1;
        }
        dest = ph->p_vaddr + bias;
        if (dest < min_dest || dest >= USER_HI || ph->p_memsz >= USER_HI - dest) {
            return -1;
        }
        if (ph->p_filesz > ph->p_memsz || ph->p_offset > lim ||
            ph->p_filesz > lim - ph->p_offset) {
            return -1;
        }
        (void)pmm_mark_reserved((metta_phys_addr_t)(dest & ~0xFFFull),
                                (ph->p_memsz + 0x1FFFu) & ~0xFFFull);
        if (vmm_allow_user((metta_phys_addr_t)dest, ph->p_memsz) != METTA_OK) {
            return -1;
        }
        dst = (uint8_t *)(uintptr_t)dest;
        memset(dst, 0, (size_t)ph->p_memsz);
        if (ph->p_filesz > 0) {
            memcpy(dst, image + ph->p_offset, (size_t)ph->p_filesz);
        }
        if (dest + ph->p_memsz > image_end) {
            image_end = dest + ph->p_memsz;
        }
        saw_load = 1;
    }

    /* ld.so coloca el primer link_map en el resto de la última página,
       que el kernel dejaría a cero. Sin eso, un objeto sin PT_PHDR
       (libdl) conserva un l_phdr basura y el caminado de cabeceras falla. */
    if (saw_load && image_end < USER_HI) {
        uint64_t clear_to = (image_end + 0x1FFFu) & ~0xFFFull;

        if (clear_to > USER_HI) {
            clear_to = USER_HI;
        }
        if (clear_to > image_end) {
            memset((void *)(uintptr_t)image_end, 0, (size_t)(clear_to - image_end));
        }
    }

    if (!saw_load || bias > UINT64_MAX - eh->e_entry) {
        return -1;
    }
    entry = eh->e_entry + bias;
    if (entry < min_dest || entry >= USER_HI) {
        return -1;
    }
    *entry_out = entry;
    *phdr_out  = phdr_va;
    *phnum_out = eh->e_phnum;
    return 0;
}

static int elf_load(const uint8_t *image, size_t size, uint64_t *entry_out)
{
    uint64_t entry = 0;
    uint64_t phdr = 0;
    uint64_t phnum = 0;

    if (linux_user_map_elf(image, size, 0, &entry, &phdr, &phnum) != 0) {
        return -1;
    }
    (void)phdr;
    (void)phnum;
    (void)pmm_mark_reserved((metta_phys_addr_t)(USER_STACK - 0x20000u), 0x20000u);
    if (vmm_allow_user((metta_phys_addr_t)(USER_STACK - 0x10000u), 0x10000u) != METTA_OK) {
        return -1;
    }
    *entry_out = entry;
    return 0;
}

int linux_user_selftest(void)
{
    uint64_t entry = 0;
    size_t   bytes = (size_t)(hello_elf_end - hello_elf_start);
    int      static_status;
    int      dyn_status;

    linux_user_cpu_init();
    linux_syscall_init();
    linux_user_leave  = 0;
    linux_user_status = -1;
    linux_user_trace  = 0;
    if (elf_load(hello_elf_start, bytes, &entry) != 0) {
        KLOG_ERR("userland: ELF de prueba rechazado\n");
        return -1;
    }
    linux_user_enter(entry, USER_STACK);
    static_status = linux_user_status;
    KLOG_INFO("userland: exit %d\n", static_status);
    dyn_status = linux_user_dyn_test();
    KLOG_INFO("dyn: exit %d\n", dyn_status);
    dyn_status = linux_user_run_firefox();
    KLOG_INFO("firefox: exit %d\n", dyn_status);
    return static_status;
}

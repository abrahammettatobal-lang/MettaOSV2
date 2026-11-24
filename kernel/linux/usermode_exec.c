/*
 * MettaOS — prueba ET_DYN: dynhello en 0x2000000 (o por encima del kernel)
 * y ld-linux en 0x10000000. La entrada es el intérprete, con pila auxv Linux.
 */
#include <linux/usermode.h>
#include <linux/user_files.h>
#include <linux/user_ram.h>
#include <linux/xdisplay.h>
#include "syscall_linux.h"

#include <metta/pmm.h>
#include <metta/vmm.h>

#include <stddef.h>
#include <stdint.h>

#define USER_STACK       0x1FE00000ull
#define MAIN_BIAS_PREFER 0x02000000ull
#define INTERP_BIAS      0x10000000ull
#define STACK_MAP_BASE   (USER_STACK - 0x10000ull)
#define STACK_MAP_LEN    0x10000ull
#define PATH_AT          (USER_STACK - 256ull)
#define RANDOM_AT        (USER_STACK - 224ull)
#define ENV1_AT          (USER_STACK - 160ull)
#define ENV2_AT          (USER_STACK - 96ull)

extern const uint8_t dynhello_start[];
extern const uint8_t dynhello_end[];
extern const uint8_t ldlinux_start[];
extern const uint8_t ldlinux_end[];
extern const uint8_t appruntime_start[];
extern const uint8_t appruntime_end[];

extern char __kernel_image_end[];

enum {
    AT_NULL   = 0,
    AT_PHDR   = 3,
    AT_PHENT  = 4,
    AT_PHNUM  = 5,
    AT_PAGESZ = 6,
    AT_BASE   = 7,
    AT_FLAGS  = 8,
    AT_ENTRY  = 9,
    AT_UID    = 11,
    AT_EUID   = 12,
    AT_GID    = 13,
    AT_EGID   = 14,
    AT_HWCAP  = 16,
    AT_CLKTCK = 17,
    AT_SECURE = 23,
    AT_RANDOM = 25,
    AT_EXECFN = 31
};

/* Siguiente frontera de 2 MiB en o después de __kernel_image_end. */
static uint64_t kernel_image_floor(void)
{
    uint64_t end = (uint64_t)(uintptr_t)(void *)__kernel_image_end;
    const uint64_t align = 0x200000ull;

    return (end + align - 1ull) & ~(align - 1ull);
}

static uint64_t dyn_main_bias(void)
{
    uint64_t floor = kernel_image_floor();

    return floor > MAIN_BIAS_PREFER ? floor : MAIN_BIAS_PREFER;
}

/*
 * Pila en USER_STACK, creciendo hacia abajo.
 * Cadena en USER_STACK-256, 16 bytes de AT_RANDOM en USER_STACK-224.
 * El vector (número par de u64) termina justo antes de la cadena; rsp queda
 * alineado a 16 y apunta a argc.
 */
static int copy_user_str(uint64_t dest, uint64_t limit, const char *src)
{
    size_t i;
    char  *dst = (char *)(uintptr_t)dest;

    if (src == NULL || dest >= limit) {
        return -1;
    }
    for (i = 0; src[i] != '\0'; i++) {
        if (dest + i + 1ull >= limit) {
            return -1;
        }
        dst[i] = src[i];
    }
    dst[i] = '\0';
    return 0;
}

static int linux_user_build_stack(uint64_t phdr, uint64_t phnum, uint64_t main_entry,
                                  const char *path, const char *env1, const char *env2,
                                  uint64_t *rsp_out)
{
    static const uint8_t random_bytes[16] = {
        0x6d, 0x65, 0x74, 0x74, 0x61, 0x6f, 0x73, 0x2d,
        0x61, 0x74, 0x72, 0x61, 0x6e, 0x64, 0x30, 0x31
    };
    uint64_t words[42];
    uint64_t rsp;
    size_t   n = 0;
    size_t   i;
    uint8_t *rand_dst;
    uint64_t *slot;

    if (path == NULL) {
        return -1;
    }
    if (copy_user_str(PATH_AT, RANDOM_AT, path) != 0) {
        return -1;
    }
    if (env1 != NULL && copy_user_str(ENV1_AT, ENV2_AT, env1) != 0) {
        return -1;
    }
    if (env2 != NULL && copy_user_str(ENV2_AT, USER_STACK, env2) != 0) {
        return -1;
    }
    rand_dst = (uint8_t *)(uintptr_t)RANDOM_AT;
    for (i = 0; i < sizeof(random_bytes); i++) {
        rand_dst[i] = random_bytes[i];
    }

    words[n++] = 1ull;
    words[n++] = PATH_AT;
    words[n++] = 0ull;
    if (env1 != NULL) {
        words[n++] = ENV1_AT;
    }
    if (env2 != NULL) {
        words[n++] = ENV2_AT;
    }
    words[n++] = 0ull;
    words[n++] = AT_PHDR;
    words[n++] = phdr;
    words[n++] = AT_PHENT;
    words[n++] = 56ull;
    words[n++] = AT_PHNUM;
    words[n++] = phnum;
    words[n++] = AT_PAGESZ;
    words[n++] = 4096ull;
    words[n++] = AT_BASE;
    words[n++] = INTERP_BIAS;
    words[n++] = AT_FLAGS;
    words[n++] = 0ull;
    words[n++] = AT_ENTRY;
    words[n++] = main_entry;
    words[n++] = AT_UID;
    words[n++] = 1000ull;
    words[n++] = AT_EUID;
    words[n++] = 1000ull;
    words[n++] = AT_GID;
    words[n++] = 1000ull;
    words[n++] = AT_EGID;
    words[n++] = 1000ull;
    words[n++] = AT_HWCAP;
    words[n++] = 0x07808111ull;
    words[n++] = AT_CLKTCK;
    words[n++] = 100ull;
    words[n++] = AT_SECURE;
    words[n++] = 0ull;
    words[n++] = AT_RANDOM;
    words[n++] = RANDOM_AT;
    words[n++] = AT_EXECFN;
    words[n++] = PATH_AT;
    words[n++] = AT_NULL;
    words[n++] = 0ull;
    if ((n & 1u) != 0u) {
        words[n++] = 0ull;
    }

    rsp = PATH_AT - n * sizeof(uint64_t);
    if ((rsp & 0xFull) != 0ull || rsp < STACK_MAP_BASE || n > (sizeof(words) / sizeof(words[0]))) {
        return -1;
    }
    slot = (uint64_t *)(uintptr_t)rsp;
    for (i = 0; i < n; i++) {
        slot[i] = words[i];
    }
    *rsp_out = rsp;
    return 0;
}

int linux_user_dyn_test(void)
{
    uint64_t bias;
    uint64_t main_entry = 0;
    uint64_t phdr = 0;
    uint64_t phnum = 0;
    uint64_t interp_entry = 0;
    uint64_t interp_phdr = 0;
    uint64_t interp_phnum = 0;
    uint64_t rsp = 0;
    size_t   main_sz;
    size_t   interp_sz;

    if (linux_user_files_init() != 0) {
        return -1;
    }
    linux_user_set_exec_path("/home/usuario/dynhello");

    bias = dyn_main_bias();
    main_sz = (size_t)(dynhello_end - dynhello_start);
    if (linux_user_map_elf(dynhello_start, main_sz, bias, &main_entry, &phdr, &phnum) != 0) {
        return -1;
    }

    interp_sz = (size_t)(ldlinux_end - ldlinux_start);
    if (linux_user_map_elf(ldlinux_start, interp_sz, INTERP_BIAS, &interp_entry,
                           &interp_phdr, &interp_phnum) != 0) {
        return -1;
    }
    (void)interp_phdr;
    (void)interp_phnum;

    (void)pmm_mark_reserved((metta_phys_addr_t)STACK_MAP_BASE, STACK_MAP_LEN);
    if (vmm_allow_user((metta_phys_addr_t)STACK_MAP_BASE, STACK_MAP_LEN) != METTA_OK) {
        return -1;
    }
    if (linux_user_build_stack(phdr, phnum, main_entry, "/home/usuario/dynhello",
                               NULL, NULL, &rsp) != 0) {
        return -1;
    }

    linux_user_trace  = 1;
    linux_user_leave  = 0;
    linux_user_status = -1;
    linux_user_enter(interp_entry, rsp);
    return linux_user_status;
}

int linux_user_run_firefox(void)
{
    const uint8_t *bin;
    uint64_t bin_sz = 0;
    uint64_t bias;
    uint64_t main_entry = 0;
    uint64_t phdr = 0;
    uint64_t phnum = 0;
    uint64_t interp_entry = 0;
    uint64_t interp_phdr = 0;
    uint64_t interp_phnum = 0;
    uint64_t rsp = 0;
    size_t   interp_sz;

    linux_user_cpu_init();
    linux_syscall_init();
    linux_user_ram_init();
    linux_xdisplay_reset();
    if (linux_user_files_init() != 0 || linux_user_appimage_load() != 0) {
        return -1;
    }
    (void)linux_user_file_find("/home/usuario/firefox-bin");
    bin = linux_user_squash_file("firefox-bin", &bin_sz);
    if (bin == NULL || bin_sz == 0u || bin_sz > (uint64_t)(size_t)-1) {
        return -1;
    }
    linux_user_set_exec_path("/home/usuario/firefox-bin");
    linux_user_mmap_reset();

    bias = dyn_main_bias();
    if (linux_user_map_elf(bin, (size_t)bin_sz, bias, &main_entry, &phdr, &phnum) != 0) {
        return -1;
    }
    interp_sz = (size_t)(ldlinux_end - ldlinux_start);
    if (linux_user_map_elf(ldlinux_start, interp_sz, INTERP_BIAS, &interp_entry,
                           &interp_phdr, &interp_phnum) != 0) {
        return -1;
    }
    (void)interp_phdr;
    (void)interp_phnum;

    (void)pmm_mark_reserved((metta_phys_addr_t)STACK_MAP_BASE, STACK_MAP_LEN);
    if (vmm_allow_user((metta_phys_addr_t)STACK_MAP_BASE, STACK_MAP_LEN) != METTA_OK) {
        return -1;
    }
    if (linux_user_build_stack(phdr, phnum, main_entry, "/home/usuario/firefox-bin",
                               "HOME=/home/usuario", "DISPLAY=:0", &rsp) != 0) {
        return -1;
    }

    linux_user_trace_reset();
    linux_user_trace  = 1;
    linux_user_leave  = 0;
    linux_user_status = -1;
    linux_user_enter(interp_entry, rsp);
    return linux_user_status;
}

int linux_user_apprun_test(void)
{
    return linux_user_run_firefox();
}

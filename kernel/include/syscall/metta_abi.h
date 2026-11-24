#ifndef METTA_SYSCALL_METTA_ABI_H
#define METTA_SYSCALL_METTA_ABI_H

#include <stddef.h>
#include <stdint.h>

/*
 * MettaOS — ABI nativa de syscalls (no Linux).
 * Compartida kernel ↔ apps .mettaapp (futuro).
 */

#define METTA_ABI_REV           1u

/* --- Convención x86_64 (instrucción SYSCALL) --- */
/*
 * Entrada (registros, inalterados respecto al hardware):
 *   rax = número de syscall (METTA_SYS_*)
 *   rdi, rsi, rdx, r10, r8, r9 = argumentos 0..5
 * Salida:
 *   rax = valor de retorno (≥0 éxito; <0 código METTA_ABI_E*)
 *
 * El stub de entrada en SA-05/09 guardará/restaurará rcx, r11 según el manual Intel.
 */

#define METTA_SYSCALL_MAX_ARGS    6u
#define METTA_SYSCALL_TABLE_SIZE  256u

/* Rango virtual usuario (canonical low half; coherente con boot-map.md) */
#define METTA_USER_ADDR_MIN       0x0000000000001000ULL
#define METTA_USER_ADDR_MAX       0x00007FFFFFFFFFFFULL

/* Acceso requerido en validación de punteros */
#define METTA_PTR_ACCESS_READ     (1u << 0)
#define METTA_PTR_ACCESS_WRITE    (1u << 1)

/* Códigos de retorno en syscall (negativos; distintos de errno Linux) */
#define METTA_ABI_OK              0
#define METTA_ABI_EINVAL          (-1)
#define METTA_ABI_EFAULT          (-2)
#define METTA_ABI_ENOSYS          (-3)
#define METTA_ABI_EPERM           (-4)
#define METTA_ABI_EBUSY           (-5)
#define METTA_ABI_ENOMEM          (-6)
#define METTA_ABI_ENOTSUP         (-7)

/* --- Números de syscall: núcleo --- */
#define METTA_SYS_NOP             0u
#define METTA_SYS_EXIT            1u   /* void exit(int status) */
#define METTA_SYS_YIELD           2u   /* ceder CPU (scheduler) */
#define METTA_SYS_GETPID          3u
#define METTA_SYS_WRITE           4u   /* ssize write(fd, buf, len) — stub */
#define METTA_SYS_READ            5u   /* ssize read(fd, buf, len) — stub */

/* Banda reservada proceso / hilos (SA-09) */
#define METTA_SYS_PROC_BASE       0x010u
#define METTA_SYS_PROC_FORK       (METTA_SYS_PROC_BASE + 0u)
#define METTA_SYS_PROC_WAIT       (METTA_SYS_PROC_BASE + 1u)

/*
 * Banda MDE / servicios gráficos (SA-15+, window manager).
 * Contrato: solo procesos con cap MDE llaman aquí; el dispatcher rechaza si no hay sesión.
 */
#define METTA_SYS_MDE_BASE        0x100u
#define METTA_SYS_MDE_HELLO       (METTA_SYS_MDE_BASE + 0u) /* handshake compositor */
#define METTA_SYS_MDE_WIN_OPEN    (METTA_SYS_MDE_BASE + 1u)
#define METTA_SYS_MDE_WIN_CLOSE   (METTA_SYS_MDE_BASE + 2u)
#define METTA_SYS_MDE_WIN_RESIZE  (METTA_SYS_MDE_BASE + 3u)
#define METTA_SYS_MDE_FB_BLIT     (METTA_SYS_MDE_BASE + 4u) /* futuro: DMA a FB */

/* Linux ABI shim vive en banda distinta (SA futuro); no reutilizar 0x200-0x3FF aquí */
#define METTA_SYS_LINUX_BASE      0x200u

typedef int64_t metta_syscall_ret_t;

typedef metta_syscall_ret_t (*metta_syscall_fn_t)(
    uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5);

typedef struct {
    uint32_t            nr;
    const char         *name;   /* diagnóstico / printk (solo kernel) */
    metta_syscall_fn_t  handler;
} metta_syscall_desc_t;

/* Dispatcher — invocado desde trap SYSCALL (syscall.c) */
void              metta_syscall_init(void);
metta_syscall_ret_t metta_syscall_dispatch(uint32_t nr,
                                              uint64_t a0, uint64_t a1, uint64_t a2,
                                              uint64_t a3, uint64_t a4, uint64_t a5);

/*
 * Validación de punteros usuario (stub hasta VMM/SA-08).
 * Comprueba canonicalidad, rango y desbordamiento; no consulta PTE todavía.
 * Devuelve METTA_ABI_OK o METTA_ABI_EFAULT / METTA_ABI_EINVAL.
 */
int metta_user_ptr_validate(const void *ptr, size_t len, uint32_t access);
int metta_user_cstr_validate(const char *str, size_t max_len);

/* Contexto del proceso actual (relleno por SA-09; por defecto kernel stub) */
uint64_t metta_syscall_current_pid(void);
int      metta_syscall_is_user_context(void);

/* Invocado por SA-09 al entrar/salir de código usuario */
void metta_syscall_set_context(uint64_t pid, int is_user);

#endif /* METTA_SYSCALL_METTA_ABI_H */
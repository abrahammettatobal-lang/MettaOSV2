#ifndef METTA_LINUX_USERMODE_H
#define METTA_LINUX_USERMODE_H

/*
 * Entrada a ring 3 (syscall/sysret) y carga de ELF ET_EXEC / ET_DYN.
 * linux_user_selftest ejecuta el ELF estático y después el hello dinámico.
 */
#include <stddef.h>
#include <stdint.h>

void linux_user_cpu_init(void);
void linux_user_note_exit(int status);
int  linux_user_in_process(void);
void linux_user_fault_leave(void) __attribute__((noreturn));
int  linux_user_selftest(void);

/*
 * Copia los PT_LOAD de un ELF64 ET_EXEC o ET_DYN a p_vaddr+bias.
 * No mapea la pila y no salta. entry = e_entry+bias.
 * phdr es bias+e_phoff, o bias+p_vaddr si hay PT_PHDR.
 */
int linux_user_map_elf(const uint8_t *image, size_t size, uint64_t bias,
                       uint64_t *entry_out, uint64_t *phdr_out, uint64_t *phnum_out);

/* Carga dynhello + ld-linux y entra por el intérprete. Devuelve el exit. */
int linux_user_dyn_test(void);

/* Runtime ELF del AppImage de Firefox, en 0x400000, por el mismo intérprete. */
int linux_user_apprun_test(void);

/* firefox-bin del AppImage, con ld.so y las bibliotecas del SquashFS. */
int linux_user_run_firefox(void);

/* Pone a cero el contador de las primeras syscalls trazadas. */
void linux_user_trace_reset(void);

/* La siguiente biblioteca anónima vuelve a 1 GiB, por encima del BSS. */
void linux_user_mmap_reset(void);
/* Fin del hueco de mmap anónimo, para /proc/self/maps. */
uint64_t linux_user_mmap_end(void);

void linux_user_enter(uint64_t entry, uint64_t user_rsp);

/*
 * clone de un solo hilo, síncrono: el hijo corre hasta exit y recién ahí
 * la syscall del padre devuelve el tid. No hay planificador.
 */
int linux_user_clone_enter(uint64_t stack, uint64_t flags, uint64_t parent_tid,
                           uint64_t child_tid, uint64_t tls);
int linux_user_clone_exit(int64_t *tid_out);
void linux_user_clone_cancel(void);
/* El hijo dormido cede al padre. 1 = *ret_out es el resultado de esta syscall. */
int linux_user_futex_block(uint64_t uaddr, int64_t *ret_out);
int linux_user_futex_wake(uint64_t uaddr, int64_t *ret_out);
int linux_user_current_tid(void);

extern int linux_user_trace;
extern volatile uint32_t linux_user_leave;
extern int32_t linux_user_status;

/* Lo define otro agente. nr es rax, a0 es rdi, ret es el valor de la syscall. */
void linux_user_trace_result(uint64_t nr, uint64_t a0, int64_t ret);

#endif /* METTA_LINUX_USERMODE_H */

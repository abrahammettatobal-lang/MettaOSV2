/* Tipos y constantes exclusivos del subsistema proc (sin duplicar metta/types.h). */
#ifndef METTA_PROC_PROC_TYPES_H
#define METTA_PROC_PROC_TYPES_H

#include "metta/types.h"
#include "metta/vmm.h"

typedef metta_u64 metta_pid_t;
#define METTA_PID_KERNEL  0ull
#define METTA_PID_INVALID UINT64_MAX

/*
 * Rango usuario (memory-ownership.md §5): tope de pila fijado aquí;
 * gap de 1 página bajo el stack queda sin mapear.
 */
#define METTA_USER_STACK_TOP  0x00007FFFFFFFF000ull
#define METTA_USER_MMAP_LOW   0x0000000000400000ull
#define METTA_USER_STACK_SIZE (64u * 1024u)

typedef enum {
    METTA_PROC_UNUSED = 0,
    METTA_PROC_READY,
    METTA_PROC_RUNNING,
    METTA_PROC_BLOCKED,
    METTA_PROC_ZOMBIE
} metta_proc_state_t;

typedef struct {
    metta_address_space_t *address_space;
    metta_asid_t           asid;
    metta_virt_addr_t      user_stack_top;
    metta_virt_addr_t      brk;
} metta_proc_mm_t;

typedef struct {
    metta_u64 rsp;
} metta_cpu_context_t;

struct metta_process {
    metta_pid_t           pid;
    metta_proc_state_t    state;
    metta_proc_mm_t       mm;
    bool                  on_runqueue;
    struct metta_process *rq_next;

    metta_u8             *kstack;
    metta_u64             kstack_size;
    metta_cpu_context_t   ctx;

    void (*entry)(void *arg);
    void                 *entry_arg;
    metta_u64             exit_code;
};

typedef struct metta_process metta_process_t;

#define METTA_PROC_KSTACK_SIZE (16u * 1024u)
#define METTA_PROC_MAX         32u

#endif /* METTA_PROC_PROC_TYPES_H */
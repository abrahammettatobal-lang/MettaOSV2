/* Procesos — PCB y memoria de proceso (contracts.md §4 + SA-09). */
#ifndef METTA_PROC_PROCESS_H
#define METTA_PROC_PROCESS_H

#include "proc/proc_types.h"

metta_status_t proc_table_init(void);
metta_process_t *proc_current(void);

metta_status_t proc_create_kernel(void (*entry)(void *), void *arg,
                                  metta_process_t **out);
metta_status_t proc_mark_zombie(metta_process_t *proc, metta_u64 code);
metta_status_t proc_destroy(metta_process_t *proc);

metta_status_t proc_mm_init(metta_process_t *proc);
metta_status_t proc_mm_clone(const metta_process_t *parent, metta_process_t *child);
metta_status_t proc_mm_destroy(metta_process_t *proc);

metta_status_t sched_switch_mm(metta_process_t *next);

#endif /* METTA_PROC_PROCESS_H */
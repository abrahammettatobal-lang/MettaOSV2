/* Planificador cooperativo (SA-09). */
#ifndef METTA_PROC_SCHEDULER_H
#define METTA_PROC_SCHEDULER_H

#include "metta/sync.h"
#include "proc/process.h"

void scheduler_init(void);
metta_status_t scheduler_run_selftest(void);

void scheduler_runqueue_lock(void);
void scheduler_runqueue_unlock(void);

void scheduler_enqueue(metta_process_t *proc);
void scheduler_dequeue(metta_process_t *proc);

void sched_cooperative_yield(void);

void scheduler_task_trampoline(void) __attribute__((noreturn));

#endif /* METTA_PROC_SCHEDULER_H */
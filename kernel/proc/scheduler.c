/* Planificador cooperativo + context switch x86_64. */
#include "proc/scheduler.h"

#include <stdint.h>

/* Declaraciones internas process.c ↔ scheduler.c */
bool scheduler_kstack_setup(metta_process_t *proc);
void scheduler_kstack_teardown(metta_process_t *proc);
void proc_set_current(metta_process_t *proc);

static metta_spinlock_t runqueue_lock = METTA_SPINLOCK_INIT;

static metta_process_t *rq_head;
static metta_process_t *rq_tail;
static int                sched_selftest_counter_a;
static int                sched_selftest_counter_b;

void scheduler_runqueue_lock(void)
{
    while (runqueue_lock.owner != 0u) {
        __asm__ volatile("pause");
    }
    runqueue_lock.owner = 1u;
}

void scheduler_runqueue_unlock(void)
{
    runqueue_lock.owner = 0u;
}

void scheduler_enqueue(metta_process_t *proc)
{
    if (proc == (metta_process_t *)0 || proc->on_runqueue) {
        return;
    }

    scheduler_runqueue_lock();
    proc->rq_next   = (metta_process_t *)0;
    proc->on_runqueue = true;
    if (rq_tail == (metta_process_t *)0) {
        rq_head = rq_tail = proc;
    } else {
        rq_tail->rq_next = proc;
        rq_tail          = proc;
    }
    proc->state = METTA_PROC_READY;
    scheduler_runqueue_unlock();
}

void scheduler_dequeue(metta_process_t *proc)
{
    metta_process_t **pp;

    if (proc == (metta_process_t *)0 || !proc->on_runqueue) {
        return;
    }

    scheduler_runqueue_lock();
    for (pp = &rq_head; *pp != (metta_process_t *)0; pp = &(*pp)->rq_next) {
        if (*pp == proc) {
            *pp = proc->rq_next;
            proc->on_runqueue = false;
            proc->rq_next     = (metta_process_t *)0;
            rq_tail           = rq_head;
            if (rq_tail != (metta_process_t *)0) {
                while (rq_tail->rq_next != (metta_process_t *)0) {
                    rq_tail = rq_tail->rq_next;
                }
            }
            break;
        }
    }
    scheduler_runqueue_unlock();
}

static metta_process_t *scheduler_pick_next(void)
{
    metta_process_t *next;

    scheduler_runqueue_lock();
    next = rq_head;
    if (next != (metta_process_t *)0) {
        rq_head = next->rq_next;
        if (rq_head == (metta_process_t *)0) {
            rq_tail = (metta_process_t *)0;
        }
        next->on_runqueue = false;
        next->rq_next     = (metta_process_t *)0;
    }
    scheduler_runqueue_unlock();
    return next;
}

static void scheduler_requeue_current(metta_process_t *proc)
{
    if (proc == (metta_process_t *)0) {
        return;
    }
    if (proc->pid != METTA_PID_KERNEL && proc->state == METTA_PROC_RUNNING) {
        proc->state = METTA_PROC_READY;
        scheduler_enqueue(proc);
    }
}

void scheduler_task_trampoline(void)
{
    metta_process_t *self = proc_current();

    if (self == (metta_process_t *)0 || self->entry == (void (*)(void *))0) {
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    self->entry(self->entry_arg);
    (void)proc_mark_zombie(self, 0);
    sched_cooperative_yield();
    for (;;) {
        __asm__ volatile("hlt");
    }
}

/*
 * Interruptor cooperativo: alineación ABI SysV de 16 bytes antes del ret.
 * Guarda rbx, rbp, r12–r15 y rsp en prev; restaura desde next.
 */
static void ctx_switch(metta_cpu_context_t *prev, metta_cpu_context_t *next)
{
    __asm__ volatile(
        "pushq %%rbx\n"
        "pushq %%rbp\n"
        "pushq %%r12\n"
        "pushq %%r13\n"
        "pushq %%r14\n"
        "pushq %%r15\n"
        "mov %%rsp, (%0)\n"
        "mov (%1), %%rsp\n"
        "popq %%r15\n"
        "popq %%r14\n"
        "popq %%r13\n"
        "popq %%r12\n"
        "popq %%rbp\n"
        "popq %%rbx\n"
        "retq\n"
        :
        : "r"(prev), "r"(next)
        : "memory");
}

bool scheduler_kstack_setup(metta_process_t *proc)
{
    static metta_u8 stacks[METTA_PROC_MAX - 1u][METTA_PROC_KSTACK_SIZE];
    static metta_u32 stack_alloc;
    metta_u64 *sp;
    metta_u64  i;

    if (stack_alloc >= METTA_PROC_MAX - 1u) {
        return false;
    }

    proc->kstack      = stacks[stack_alloc];
    proc->kstack_size = METTA_PROC_KSTACK_SIZE;
    stack_alloc++;

    sp = (metta_u64 *)(proc->kstack + proc->kstack_size);
    while (((uintptr_t)sp & 0xFu) != 0u) {
        sp--;
        *sp = 0;
    }

    for (i = 0; i < 6u; i++) {
        sp--;
        *sp = 0;
    }

    sp--;
    *sp = (metta_u64)(uintptr_t)scheduler_task_trampoline;

    proc->ctx.rsp = (metta_u64)(uintptr_t)sp;
    return true;
}

void scheduler_kstack_teardown(metta_process_t *proc)
{
    proc->kstack      = (metta_u8 *)0;
    proc->kstack_size = 0;
    proc->ctx.rsp     = 0;
}

void sched_cooperative_yield(void)
{
    metta_process_t *prev = proc_current();
    metta_process_t *next;
    metta_status_t   mmst;

    if (prev == (metta_process_t *)0) {
        return;
    }

    scheduler_requeue_current(prev);

    for (;;) {
        next = scheduler_pick_next();
        if (next == (metta_process_t *)0) {
            if (prev->state == METTA_PROC_READY) {
                prev->state = METTA_PROC_RUNNING;
            }
            return;
        }

        if (next == prev) {
            prev->state = METTA_PROC_RUNNING;
            return;
        }

        mmst = sched_switch_mm(next);
        if (mmst != METTA_OK && next->mm.asid != METTA_ASID_KERNEL) {
            (void)proc_mark_zombie(next, (metta_u64)mmst);
            continue;
        }

        prev->state = METTA_PROC_READY;
        next->state = METTA_PROC_RUNNING;
        proc_set_current(next);

        ctx_switch(&prev->ctx, &next->ctx);

        prev = proc_current();
        return;
    }
}

void scheduler_init(void)
{
    (void)proc_table_init();
    rq_head = rq_tail = (metta_process_t *)0;
}

static void selftest_task_a(void *arg)
{
    (void)arg;
    for (int i = 0; i < 4; i++) {
        sched_selftest_counter_a++;
        sched_cooperative_yield();
    }
    (void)proc_mark_zombie(proc_current(), 0);
    sched_cooperative_yield();
}

static void selftest_task_b(void *arg)
{
    (void)arg;
    for (int i = 0; i < 4; i++) {
        sched_selftest_counter_b++;
        sched_cooperative_yield();
    }
    (void)proc_mark_zombie(proc_current(), 0);
    sched_cooperative_yield();
}

metta_status_t scheduler_run_selftest(void)
{
    metta_process_t *a;
    metta_process_t *b;
    metta_status_t   st;
    int              spins;
    metta_u64        start;

    sched_selftest_counter_a = 0;
    sched_selftest_counter_b = 0;

    st = proc_create_kernel(selftest_task_a, (void *)0, &a);
    if (st != METTA_OK) {
        return st;
    }
    st = proc_create_kernel(selftest_task_b, (void *)0, &b);
    if (st != METTA_OK) {
        return st;
    }

    start = 0;
    spins = 0;
    while ((sched_selftest_counter_a < 4 || sched_selftest_counter_b < 4) &&
           spins < 256) {
        sched_cooperative_yield();
        spins++;
        start++;
        (void)start;
    }

    if (sched_selftest_counter_a != 4 || sched_selftest_counter_b != 4) {
        return METTA_ERR_INTERNAL;
    }

    st = proc_destroy(a);
    if (st != METTA_OK) {
        return st;
    }
    return proc_destroy(b);
}
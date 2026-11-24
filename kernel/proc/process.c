/* PCB, memoria de proceso y sched_switch_mm (contracts.md §4). */
#include "proc/process.h"
#include "proc/scheduler.h"

#include <stdbool.h>
#include <stdint.h>

bool scheduler_kstack_setup(metta_process_t *proc);
void scheduler_kstack_teardown(metta_process_t *proc);
void proc_set_current(metta_process_t *proc);

/*
 * Enlaces débiles: SA-08 sustituye por implementación real sin tocar proc/.
 */
__attribute__((weak)) metta_status_t vmm_as_create(metta_address_space_t **out,
                                                   metta_asid_t *asid_out)
{
    (void)out;
    (void)asid_out;
    return METTA_ERR_UNINITIALIZED;
}

__attribute__((weak)) metta_status_t vmm_as_destroy(metta_address_space_t *as)
{
    (void)as;
    return METTA_ERR_UNINITIALIZED;
}

__attribute__((weak)) metta_status_t vmm_map(metta_address_space_t *as,
                                             const metta_map_request_t *req)
{
    (void)as;
    (void)req;
    return METTA_ERR_UNINITIALIZED;
}

__attribute__((weak)) metta_status_t vmm_switch_to(metta_address_space_t *as)
{
    (void)as;
    return METTA_ERR_UNINITIALIZED;
}

__attribute__((weak)) metta_address_space_t *vmm_current_as(void)
{
    return (metta_address_space_t *)0;
}

static metta_process_t proc_table[METTA_PROC_MAX];
static metta_process_t *proc_current_task;
static metta_pid_t       proc_next_pid = 1ull;

static metta_spinlock_t proc_table_lock = METTA_SPINLOCK_INIT;

metta_process_t *proc_current(void)
{
    return proc_current_task;
}

void proc_set_current(metta_process_t *proc)
{
    proc_current_task = proc;
}

metta_status_t proc_table_init(void)
{
    for (metta_u32 i = 0; i < METTA_PROC_MAX; i++) {
        proc_table[i].pid            = METTA_PID_INVALID;
        proc_table[i].state          = METTA_PROC_UNUSED;
        proc_table[i].mm.address_space = (metta_address_space_t *)0;
        proc_table[i].mm.asid        = METTA_ASID_INVALID;
        proc_table[i].mm.user_stack_top = METTA_VIRT_NONE;
        proc_table[i].mm.brk         = METTA_USER_MMAP_LOW;
        proc_table[i].on_runqueue    = false;
        proc_table[i].rq_next        = (metta_process_t *)0;
        proc_table[i].kstack         = (metta_u8 *)0;
        proc_table[i].kstack_size    = 0;
        proc_table[i].ctx.rsp        = 0;
        proc_table[i].entry          = (void (*)(void *))0;
        proc_table[i].entry_arg      = (void *)0;
        proc_table[i].exit_code      = 0;
    }

    proc_table[0].pid         = METTA_PID_KERNEL;
    proc_table[0].state       = METTA_PROC_RUNNING;
    proc_table[0].mm.asid     = METTA_ASID_KERNEL;
    proc_table[0].mm.brk      = METTA_USER_MMAP_LOW;
    proc_current_task         = &proc_table[0];
    return METTA_OK;
}

static metta_process_t *proc_alloc_slot(void)
{
    for (metta_u32 i = 1; i < METTA_PROC_MAX; i++) {
        if (proc_table[i].state == METTA_PROC_UNUSED) {
            return &proc_table[i];
        }
    }
    return (metta_process_t *)0;
}

metta_status_t sched_switch_mm(metta_process_t *next)
{
    if (next == (metta_process_t *)0) {
        return METTA_ERR_PERM;
    }

    if (next->mm.asid == METTA_ASID_KERNEL) {
        return METTA_OK;
    }

    if (next->mm.address_space == (metta_address_space_t *)0) {
        return METTA_ERR_PERM;
    }

    if (vmm_current_as() != next->mm.address_space) {
        return vmm_switch_to(next->mm.address_space);
    }

    return METTA_OK;
}

metta_status_t proc_mm_init(metta_process_t *proc)
{
    metta_address_space_t *as = (metta_address_space_t *)0;
    metta_asid_t           asid;
    metta_map_request_t    map_req;
    metta_virt_addr_t      stack_bottom;

    if (proc == (metta_process_t *)0) {
        return METTA_ERR_INVALID_ARG;
    }
    if (proc->mm.asid == METTA_ASID_KERNEL) {
        return METTA_ERR_PERM;
    }
    if (proc->mm.address_space != (metta_address_space_t *)0) {
        return METTA_ERR_BUSY;
    }

    metta_status_t st = vmm_as_create(&as, &asid);
    if (st != METTA_OK) {
        return st;
    }
    if (asid < 1u) {
        (void)vmm_as_destroy(as);
        return METTA_ERR_CORRUPT;
    }

    proc->mm.address_space  = as;
    proc->mm.asid           = asid;
    proc->mm.user_stack_top = METTA_USER_STACK_TOP;
    proc->mm.brk            = METTA_USER_MMAP_LOW;

    stack_bottom = proc->mm.user_stack_top - METTA_USER_STACK_SIZE;
    map_req.virt  = stack_bottom;
    map_req.phys  = (metta_phys_frame_t *)0;
    map_req.perms = METTA_PERM_READ | METTA_PERM_WRITE | METTA_PERM_USER;
    map_req.flags = METTA_MAP_NOEXEC;

    st = vmm_map(as, &map_req);
    if (st != METTA_OK) {
        (void)vmm_as_destroy(as);
        proc->mm.address_space  = (metta_address_space_t *)0;
        proc->mm.asid           = METTA_ASID_INVALID;
        proc->mm.user_stack_top = METTA_VIRT_NONE;
        return st;
    }

    return METTA_OK;
}

metta_status_t proc_mm_clone(const metta_process_t *parent, metta_process_t *child)
{
    (void)parent;
    (void)child;
    return METTA_ERR_INTERNAL;
}

metta_status_t proc_mm_destroy(metta_process_t *proc)
{
    if (proc == (metta_process_t *)0) {
        return METTA_ERR_INVALID_ARG;
    }

    scheduler_runqueue_lock();

    if (proc->on_runqueue || proc->state == METTA_PROC_RUNNING) {
        scheduler_runqueue_unlock();
        return METTA_ERR_BUSY;
    }

    scheduler_runqueue_unlock();

    if (proc->mm.asid == METTA_ASID_KERNEL) {
        return METTA_ERR_PERM;
    }

    if (proc->mm.address_space == (metta_address_space_t *)0) {
        proc->mm.asid           = METTA_ASID_INVALID;
        proc->mm.user_stack_top = METTA_VIRT_NONE;
        return METTA_OK;
    }

    metta_status_t st = vmm_as_destroy(proc->mm.address_space);
    if (st != METTA_OK) {
        return st;
    }

    proc->mm.address_space  = (metta_address_space_t *)0;
    proc->mm.asid           = METTA_ASID_INVALID;
    proc->mm.user_stack_top = METTA_VIRT_NONE;
    return METTA_OK;
}

metta_status_t proc_create_kernel(void (*entry)(void *), void *arg,
                                  metta_process_t **out)
{
    metta_process_t *proc;

    if (entry == (void (*)(void *))0 || out == (metta_process_t **)0) {
        return METTA_ERR_INVALID_ARG;
    }

    (void)proc_table_lock;

    proc = proc_alloc_slot();
    if (proc == (metta_process_t *)0) {
        return METTA_ERR_NO_MEMORY;
    }

    proc->pid        = proc_next_pid++;
    proc->state      = METTA_PROC_READY;
    proc->mm.asid    = METTA_ASID_KERNEL;
    proc->mm.brk     = METTA_USER_MMAP_LOW;
    proc->on_runqueue = false;
    proc->entry      = entry;
    proc->entry_arg  = arg;
    proc->kstack_size = METTA_PROC_KSTACK_SIZE;

    if (!scheduler_kstack_setup(proc)) {
        proc->state = METTA_PROC_UNUSED;
        proc->pid   = METTA_PID_INVALID;
        return METTA_ERR_NO_MEMORY;
    }

    scheduler_enqueue(proc);
    *out = proc;
    return METTA_OK;
}

metta_status_t proc_mark_zombie(metta_process_t *proc, metta_u64 code)
{
    if (proc == (metta_process_t *)0) {
        return METTA_ERR_INVALID_ARG;
    }

    scheduler_dequeue(proc);
    proc->state     = METTA_PROC_ZOMBIE;
    proc->exit_code = code;
    return METTA_OK;
}

metta_status_t proc_destroy(metta_process_t *proc)
{
    metta_status_t st;

    if (proc == (metta_process_t *)0) {
        return METTA_ERR_INVALID_ARG;
    }
    if (proc->pid == METTA_PID_KERNEL) {
        return METTA_ERR_PERM;
    }
    if (proc->state != METTA_PROC_ZOMBIE) {
        return METTA_ERR_BUSY;
    }

    st = proc_mm_destroy(proc);
    if (st != METTA_OK) {
        return st;
    }

    scheduler_kstack_teardown(proc);
    proc->pid   = METTA_PID_INVALID;
    proc->state = METTA_PROC_UNUSED;
    return METTA_OK;
}
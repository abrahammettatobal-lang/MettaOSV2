#ifndef METTA_SYNC_H
#define METTA_SYNC_H

#include "metta/types.h"

typedef struct {
    volatile metta_u32 owner;
    volatile metta_u32 depth;
} metta_spinlock_t;

#define METTA_SPINLOCK_INIT { 0, 0 }

void metta_spin_lock(metta_spinlock_t *lock);
void metta_spin_unlock(metta_spinlock_t *lock);
bool metta_spin_trylock(metta_spinlock_t *lock);

metta_u32 metta_irq_save(void);
void      metta_irq_restore(metta_u32 flags);

#endif /* METTA_SYNC_H */
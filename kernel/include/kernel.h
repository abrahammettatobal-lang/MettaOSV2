/* MettaOS — fachada de cabeceras del kernel. */
#ifndef METTA_KERNEL_H
#define METTA_KERNEL_H

#include "metta/boot_info.h"

void kernel_halt(void) __attribute__((noreturn));

#endif /* METTA_KERNEL_H */
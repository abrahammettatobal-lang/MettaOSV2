#ifndef METTA_CPU_EXCEPTIONS_H
#define METTA_CPU_EXCEPTIONS_H

#include <stdint.h>

typedef struct {
    /* Empujados por exception_common (dirección baja → alta). */
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t unused; /* hueco del push $0 del stub */
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    /* CPU (mismo ring): RIP, CS, RFLAGS */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
} cpu_exception_frame_t;

void exception_handler(cpu_exception_frame_t *frame);

#endif /* METTA_CPU_EXCEPTIONS_H */
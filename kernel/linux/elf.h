/* MettaOS — validación y segmentos PT_LOAD ELF64 (SA-30). */
#ifndef METTA_LINUX_ELF_H
#define METTA_LINUX_ELF_H

#include <metta/types.h>

#define METTA_LINUX_ELF_REV        1u
#define METTA_LINUX_ELF_MAX_PT_LOAD 16u

typedef struct {
    metta_u64 vaddr;
    metta_u64 memsz;
    metta_u64 filesz;
    metta_u64 offset;
    metta_u64 align;
    metta_u32 flags; /* PF_R | PF_W | PF_X */
    metta_u32 reserved;
} linux_elf_pt_load_t;

typedef struct {
    metta_u16             type;     /* ET_EXEC / ET_DYN */
    metta_u16             machine;  /* EM_X86_64 */
    metta_u64             entry;
    metta_u64             load_base; /* min p_vaddr de PT_LOAD */
    metta_u64             load_hi;   /* max (p_vaddr + p_memsz) */
    metta_u8              pie;
    metta_u8              has_interp;
    metta_u8              pt_load_count;
    metta_u8              reserved;
    linux_elf_pt_load_t   pt_load[METTA_LINUX_ELF_MAX_PT_LOAD];
} linux_elf_image_t;

/* Solo cabecera + tabla de programa dentro de `size`. */
metta_status_t linux_elf_validate(const void *image, metta_u64 size);

/* Validación + recolección ordenada de PT_LOAD y metadatos de ejecución. */
metta_status_t linux_elf_parse(const void *image, metta_u64 size,
                               linux_elf_image_t *out);

/* Lectura de un segmento ya parseado (índice 0 .. pt_load_count-1). */
metta_status_t linux_elf_pt_load_get(const linux_elf_image_t *img, metta_u32 index,
                                     linux_elf_pt_load_t *seg);

/* `vaddr` debe caer en algún PT_LOAD con PF_X (p. ej. e_entry). */
metta_status_t linux_elf_vaddr_in_exec_load(const linux_elf_image_t *img,
                                            metta_u64 vaddr);

#endif /* METTA_LINUX_ELF_H */
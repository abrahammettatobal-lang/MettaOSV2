/* MettaOS — constantes y layout ELF64 (privado al módulo elf). */
#ifndef METTA_LINUX_ELF_TYPES_H
#define METTA_LINUX_ELF_TYPES_H

#include <metta/types.h>

#define ELFMAG0     0x7Fu
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'

#define ELFCLASS64  2u
#define ELFDATA2LSB 1u
#define ELFDATA2MSB 2u
#define EV_CURRENT  1u

#define ET_NONE     0u
#define ET_REL      1u
#define ET_EXEC     2u
#define ET_DYN      3u
#define ET_CORE     4u

#define EM_NONE     0u
#define EM_X86_64   62u

#define PT_NULL     0u
#define PT_LOAD     1u
#define PT_DYNAMIC  2u
#define PT_INTERP   3u
#define PT_NOTE     4u
#define PT_SHLIB    5u
#define PT_PHDR     6u
#define PT_TLS      7u
#define PT_GNU_STACK 0x6474e551u

#define PF_X        (1u << 0)
#define PF_W        (1u << 1)
#define PF_R        (1u << 2)
#define PF_MASK     (PF_X | PF_W | PF_R)

#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_OSABI    7

typedef struct __attribute__((packed)) {
    metta_u8  e_ident[16];
    metta_u16 e_type;
    metta_u16 e_machine;
    metta_u32 e_version;
    metta_u64 e_entry;
    metta_u64 e_phoff;
    metta_u64 e_shoff;
    metta_u32 e_flags;
    metta_u16 e_ehsize;
    metta_u16 e_phentsize;
    metta_u16 e_phnum;
    metta_u16 e_shentsize;
    metta_u16 e_shnum;
    metta_u16 e_shstrndx;
} metta_elf64_ehdr_t;

typedef struct __attribute__((packed)) {
    metta_u32 p_type;
    metta_u32 p_flags;
    metta_u64 p_offset;
    metta_u64 p_vaddr;
    metta_u64 p_paddr;
    metta_u64 p_filesz;
    metta_u64 p_memsz;
    metta_u64 p_align;
} metta_elf64_phdr_t;

#endif /* METTA_LINUX_ELF_TYPES_H */
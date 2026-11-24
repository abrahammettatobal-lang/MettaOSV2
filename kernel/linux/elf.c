/* MettaOS — validación ELF64 y PT_LOAD (SA-30). */
#include "elf.h"
#include "elf_types.h"

#include <stddef.h>

static int elf_u64_add_sat(metta_u64 a, metta_u64 b, metta_u64 *out)
{
    if (a > (metta_u64)(-1) - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

static int elf_range_in_image(metta_u64 off, metta_u64 len, metta_u64 image_size)
{
    metta_u64 end;
    if (len == 0) {
        return off <= image_size;
    }
    if (!elf_u64_add_sat(off, len, &end)) {
        return 0;
    }
    return end <= image_size;
}

static int elf_is_pow2_or_zero(metta_u64 v)
{
    if (v == 0) {
        return 1;
    }
    return (v & (v - 1u)) == 0;
}

static metta_status_t elf_ident_ok(const metta_elf64_ehdr_t *eh)
{
    if (eh->e_ident[EI_MAG0] != ELFMAG0 || eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 || eh->e_ident[EI_MAG3] != ELFMAG3) {
        return METTA_ERR_CORRUPT;
    }
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) {
        return METTA_ERR_INVALID_ARG;
    }
    if (eh->e_ident[EI_DATA] != ELFDATA2LSB) {
        return METTA_ERR_INVALID_ARG;
    }
    if (eh->e_ident[EI_VERSION] != EV_CURRENT) {
        return METTA_ERR_CORRUPT;
    }
    return METTA_OK;
}

static metta_status_t elf_header_ok(const metta_elf64_ehdr_t *eh, metta_u64 image_size)
{
    metta_status_t st = elf_ident_ok(eh);
    if (st != METTA_OK) {
        return st;
    }

    if (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) {
        return METTA_ERR_INVALID_ARG;
    }
    if (eh->e_machine != EM_X86_64) {
        return METTA_ERR_INVALID_ARG;
    }
    if (eh->e_version != EV_CURRENT) {
        return METTA_ERR_CORRUPT;
    }
    if (eh->e_ehsize != sizeof(metta_elf64_ehdr_t)) {
        return METTA_ERR_CORRUPT;
    }
    if (eh->e_phentsize != sizeof(metta_elf64_phdr_t)) {
        return METTA_ERR_CORRUPT;
    }
    if (eh->e_phnum == 0) {
        return METTA_ERR_CORRUPT;
    }

    if (!elf_range_in_image(eh->e_phoff,
                            (metta_u64)eh->e_phnum * sizeof(metta_elf64_phdr_t),
                            image_size)) {
        return METTA_ERR_CORRUPT;
    }

    if (image_size < sizeof(metta_elf64_ehdr_t)) {
        return METTA_ERR_INVALID_ARG;
    }

    return METTA_OK;
}

static metta_status_t elf_pt_load_sane(const metta_elf64_phdr_t *ph, metta_u64 image_size)
{
    if (ph->p_type != PT_LOAD) {
        return METTA_OK;
    }

    if ((ph->p_flags & ~PF_MASK) != 0) {
        return METTA_ERR_CORRUPT;
    }
    if ((ph->p_flags & (PF_R | PF_X)) == 0) {
        return METTA_ERR_CORRUPT;
    }

    if (ph->p_memsz < ph->p_filesz) {
        return METTA_ERR_CORRUPT;
    }

    if (!elf_range_in_image(ph->p_offset, ph->p_filesz, image_size)) {
        return METTA_ERR_CORRUPT;
    }

    if (!elf_is_pow2_or_zero(ph->p_align)) {
        return METTA_ERR_CORRUPT;
    }

    if (ph->p_align > 1) {
        metta_u64 va_mod = ph->p_vaddr & (ph->p_align - 1u);
        metta_u64 off_mod = ph->p_offset & (ph->p_align - 1u);
        if (va_mod != off_mod) {
            return METTA_ERR_CORRUPT;
        }
    }

    return METTA_OK;
}

static int elf_vaddr_contains(const linux_elf_pt_load_t *seg, metta_u64 vaddr)
{
    metta_u64 end;
    if (!elf_u64_add_sat(seg->vaddr, seg->memsz, &end)) {
        return 0;
    }
    return vaddr >= seg->vaddr && vaddr < end;
}

static void elf_image_clear(linux_elf_image_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = (linux_elf_image_t){0};
}

static void elf_sort_pt_load(linux_elf_pt_load_t *loads, metta_u8 count)
{
    for (metta_u8 i = 0; i < count; i++) {
        metta_u8 min = i;
        for (metta_u8 j = (metta_u8)(i + 1u); j < count; j++) {
            if (loads[j].vaddr < loads[min].vaddr) {
                min = j;
            }
        }
        if (min != i) {
            linux_elf_pt_load_t tmp = loads[i];
            loads[i] = loads[min];
            loads[min] = tmp;
        }
    }
}

static metta_status_t elf_pt_load_overlap_ok(const linux_elf_pt_load_t *loads, metta_u8 count)
{
    for (metta_u8 i = 0; i < count; i++) {
        metta_u64 end_i;
        if (!elf_u64_add_sat(loads[i].vaddr, loads[i].memsz, &end_i)) {
            return METTA_ERR_CORRUPT;
        }
        for (metta_u8 j = (metta_u8)(i + 1u); j < count; j++) {
            metta_u64 end_j;
            if (!elf_u64_add_sat(loads[j].vaddr, loads[j].memsz, &end_j)) {
                return METTA_ERR_CORRUPT;
            }
            metta_u64 lo = loads[i].vaddr > loads[j].vaddr ? loads[i].vaddr : loads[j].vaddr;
            metta_u64 hi = end_i < end_j ? end_i : end_j;
            if (lo < hi && loads[i].flags != loads[j].flags) {
                return METTA_ERR_CORRUPT;
            }
        }
    }
    return METTA_OK;
}

static metta_status_t elf_walk_phdrs(const void *image, metta_u64 image_size,
                                     const metta_elf64_ehdr_t *eh, linux_elf_image_t *out)
{
    metta_u8 load_count = 0;
    int saw_load = 0;

    const metta_u8 *base = (const metta_u8 *)image;
    const metta_elf64_phdr_t *phdrs =
        (const metta_elf64_phdr_t *)(base + eh->e_phoff);

    for (metta_u16 i = 0; i < eh->e_phnum; i++) {
        const metta_elf64_phdr_t *ph = &phdrs[i];

        switch (ph->p_type) {
        case PT_NULL:
            break;
        case PT_LOAD: {
            metta_status_t st = elf_pt_load_sane(ph, image_size);
            if (st != METTA_OK) {
                return st;
            }
            saw_load = 1;
            if (out != NULL) {
                if (load_count >= METTA_LINUX_ELF_MAX_PT_LOAD) {
                    return METTA_ERR_RANGE;
                }
                out->pt_load[load_count] = (linux_elf_pt_load_t){
                    .vaddr = ph->p_vaddr,
                    .memsz = ph->p_memsz,
                    .filesz = ph->p_filesz,
                    .offset = ph->p_offset,
                    .align = ph->p_align,
                    .flags = ph->p_flags,
                    .reserved = 0,
                };
                load_count++;
            }
            break;
        }
        case PT_INTERP:
            if (out != NULL) {
                if (!elf_range_in_image(ph->p_offset, ph->p_filesz, image_size)) {
                    return METTA_ERR_CORRUPT;
                }
                out->has_interp = 1;
            }
            break;
        case PT_GNU_STACK:
            /* Política MettaOS: rechazar stack ejecutable. */
            if (ph->p_flags & PF_X) {
                return METTA_ERR_CORRUPT;
            }
            break;
        default:
            break;
        }
    }

    if (!saw_load) {
        return METTA_ERR_CORRUPT;
    }

    if (out != NULL) {
        out->pt_load_count = load_count;
        elf_sort_pt_load(out->pt_load, load_count);

        out->load_base = out->pt_load[0].vaddr;
        out->load_hi = 0;
        for (metta_u8 k = 0; k < load_count; k++) {
            metta_u64 end;
            if (!elf_u64_add_sat(out->pt_load[k].vaddr, out->pt_load[k].memsz, &end)) {
                return METTA_ERR_CORRUPT;
            }
            if (end > out->load_hi) {
                out->load_hi = end;
            }
            if (out->pt_load[k].vaddr < out->load_base) {
                out->load_base = out->pt_load[k].vaddr;
            }
        }

        metta_status_t ov = elf_pt_load_overlap_ok(out->pt_load, load_count);
        if (ov != METTA_OK) {
            return ov;
        }

        out->type = eh->e_type;
        out->machine = eh->e_machine;
        out->entry = eh->e_entry;
        out->pie = (eh->e_type == ET_DYN) ? 1u : 0u;

        if (linux_elf_vaddr_in_exec_load(out, eh->e_entry) != METTA_OK) {
            return METTA_ERR_CORRUPT;
        }
    }

    return METTA_OK;
}

metta_status_t linux_elf_validate(const void *image, metta_u64 size)
{
    if (image == NULL || size < sizeof(metta_elf64_ehdr_t)) {
        return METTA_ERR_INVALID_ARG;
    }

    const metta_elf64_ehdr_t *eh = (const metta_elf64_ehdr_t *)image;
    metta_status_t st = elf_header_ok(eh, size);
    if (st != METTA_OK) {
        return st;
    }

    return elf_walk_phdrs(image, size, eh, NULL);
}

metta_status_t linux_elf_parse(const void *image, metta_u64 size, linux_elf_image_t *out)
{
    if (out == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    elf_image_clear(out);

    metta_status_t st = linux_elf_validate(image, size);
    if (st != METTA_OK) {
        return st;
    }

    const metta_elf64_ehdr_t *eh = (const metta_elf64_ehdr_t *)image;
    return elf_walk_phdrs(image, size, eh, out);
}

metta_status_t linux_elf_pt_load_get(const linux_elf_image_t *img, metta_u32 index,
                                     linux_elf_pt_load_t *seg)
{
    if (img == NULL || seg == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (index >= img->pt_load_count) {
        return METTA_ERR_RANGE;
    }
    *seg = img->pt_load[index];
    return METTA_OK;
}

metta_status_t linux_elf_vaddr_in_exec_load(const linux_elf_image_t *img, metta_u64 vaddr)
{
    if (img == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    for (metta_u8 i = 0; i < img->pt_load_count; i++) {
        const linux_elf_pt_load_t *seg = &img->pt_load[i];
        if ((seg->flags & PF_X) == 0) {
            continue;
        }
        if (elf_vaddr_contains(seg, vaddr)) {
            return METTA_OK;
        }
    }
    return METTA_ERR_RANGE;
}
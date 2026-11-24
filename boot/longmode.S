; MettaOS — mapa de identidad mínimo (páginas de 2 MiB) para Long Mode.
; Ensamblar: nasm -f elf64 boot/longmode.S

bits 32

section .text

global setup_identity_map

%define PAGE_PRESENT (1 << 0)
%define PAGE_WRITE   (1 << 1)
%define PAGE_HUGE    (1 << 7)

extern pml4
extern pdpt
extern pd
extern pd_1g
extern pd_2g
extern pd_3g
extern pd_4g
extern pd_5g

; void setup_identity_map(void)
; PML4 → PDPT → 6×PD con huge 2 MiB: identidad física 0..6 GiB (QEMU -m 6144).
; A partir de 4 GiB el PTE lleva 1 en el dword alto. Carga CR3.
; Llamar en protegido 32-bit con PAE antes de CR0.PG.
setup_identity_map:
    push eax
    push ebx
    push ecx
    push edx
    push ebp
    push edi
    push esi

    ; Limpiar 8 tablas (pml4, pdpt, 6×pd)
    mov edi, pml4
    mov ecx, (4096 * 8) / 4
    xor eax, eax
    rep stosd

    ; PML4[0] → PDPT
    mov eax, pdpt
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [pml4], eax

    ; PDPT[0..3] → pd / pd_1g / pd_2g / pd_3g
    mov eax, pd
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [pdpt], eax
    mov eax, pd_1g
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [pdpt + 8], eax
    mov eax, pd_2g
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [pdpt + 16], eax
    mov eax, pd_3g
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [pdpt + 24], eax
    mov eax, pd_4g
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [pdpt + 32], eax
    mov eax, pd_5g
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [pdpt + 40], eax

    ; Seis directorios contiguos, uno por GiB. ebp es el dword alto del PTE.
    mov esi, pd
    xor edx, edx
.fill_gb:
    mov edi, esi
    mov ebx, edx
    xor ebp, ebp
    cmp edx, 4
    jb .low_gb
    mov ebp, 1
    sub ebx, 4
.low_gb:
    shl ebx, 30
    mov ecx, 512
.fill_pd:
    mov eax, ebx
    or eax, PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE
    mov dword [edi], eax
    mov dword [edi + 4], ebp
    add edi, 8
    add ebx, 0x200000
    jnc .no_carry
    inc ebp
.no_carry:
    loop .fill_pd
    add esi, 4096
    inc edx
    cmp edx, 6
    jb .fill_gb

    mov eax, pml4
    mov cr3, eax

    pop esi
    pop edi
    pop ebp
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret
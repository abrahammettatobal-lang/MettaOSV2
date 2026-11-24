; MettaOS — cabecera Multiboot2, GDT, PAE, Long Mode, boot_enter.
; Ensamblar: nasm -f elf64 boot/multiboot2.S

section .multiboot_header
align 8
header_start:
    dd 0xE85250D6                   ; magic
    dd 0                             ; architecture (i386 / cargador)
    dd header_end - header_start
    dd -(0xE85250D6 + 0 + (header_end - header_start))

    ; Tag framebuffer (tipo 5): pedir modo gráfico lineal
    align 8
    dw 5
    dw 0
    dd 20
    dd 1024
    dd 768
    dd 32

    align 8
    ; etiqueta final obligatoria (tipo 0, tamaño 8)
    dw 0
    dw 0
    dd 8
header_end:

section .text

bits 32

global _start
global start

extern boot_enter
extern setup_identity_map

%define CR4_PAE         (1 << 5)
%define CR0_PG          (1 << 31)
%define EFER_MSR        0xC0000080
%define EFER_LME        (1 << 8)

%define CODE64_SEL      0x08
%define DATA64_SEL      0x10
%define CODE32_SEL      0x18
%define DATA32_SEL      0x20

_start:
start:
    cli

    ; Multiboot2: EAX = magic, EBX = puntero físico al MBI
    mov dword [saved_magic], eax
    mov [saved_mbi], ebx

    lea esp, [stack_top]
    and esp, ~0xF

    lgdt [gdt_desc]

    ; Recargar selectores tras LGDT (aún 32-bit protegido)
    jmp CODE32_SEL:prot_reload
prot_reload:
    mov ax, DATA32_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; PAE antes de tablas de 4 niveles / CR3
    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax

    call setup_identity_map

    ; Long Mode Enable (EFER.LME)
    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_LME
    wrmsr

    ; Paginación (PE ya activo por GRUB)
    mov eax, cr0
    or eax, CR0_PG
    mov cr0, eax

    jmp CODE64_SEL:long_mode_entry

bits 64
long_mode_entry:
    mov ax, DATA64_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    lea rsp, [rel stack_top]
    and rsp, ~0xF

    mov edi, [rel saved_magic]
    mov rsi, [rel saved_mbi]
    xor eax, eax
    xor edx, edx
    call boot_enter

.hang:
    hlt
    jmp .hang

section .rodata
align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF       ; 0x08 — código 64-bit (L=1)
    dq 0x00CF92000000FFFF       ; 0x10 — datos
    dq 0x00CF9A000000FFFF       ; 0x18 — código 32-bit (compat)
    dq 0x00CF92000000FFFF       ; 0x20 — datos 32-bit
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

section .bss
align 4096

global pml4
global pdpt
global pd
global pd_1g
global pd_2g
global pd_3g
global pd_4g
global pd_5g

pml4:
    resb 4096
pdpt:
    resb 4096
pd:
    resb 4096
pd_1g:
    resb 4096
pd_2g:
    resb 4096
pd_3g:
    resb 4096
pd_4g:
    resb 4096
pd_5g:
    resb 4096

stack_bottom:
    resb 65536
stack_top:

align 8
saved_magic:
    resq 1
saved_mbi:
    resq 1
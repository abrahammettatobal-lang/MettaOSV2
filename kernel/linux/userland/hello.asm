; ELF de prueba: write(1, msg) + exit(0). Enlazado en 0x2000000.
bits 64

section .text
global _start
_start:
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel msg]
    mov rdx, msg_len
    syscall
    mov rax, 60
    xor rdi, rdi
    syscall
    jmp _start

section .rodata
msg: db "userland ok", 10
msg_len equ $ - msg

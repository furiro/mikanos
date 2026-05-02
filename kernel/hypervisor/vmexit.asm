
bits 64
section .text

extern vmexit_handler_c

global vmexit_entry
vmexit_entry:
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    sub rsp, 8
    call vmexit_handler_c

.loop:
    hlt
    jmp .loop
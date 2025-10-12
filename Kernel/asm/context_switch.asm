GLOBAL context_switch

SECTION .text

; funcion context_switch recibe punteros a contexto viejo y nuevo
; rdi contiene puntero a contexto viejo
; rsi contiene puntero a contexto nuevo

context_switch:
    cmp rdi, 0
    je .load_new

    mov [rdi + 0],   rax
    mov [rdi + 8],   rbx
    mov [rdi + 16],  rcx
    mov [rdi + 24],  rdx
    mov [rdi + 32],  rsi
    mov [rdi + 40],  rdi
    mov [rdi + 48],  rbp
    mov [rdi + 56],  r8
    mov [rdi + 64],  r9
    mov [rdi + 72],  r10
    mov [rdi + 80],  r11
    mov [rdi + 88],  r12
    mov [rdi + 96],  r13
    mov [rdi + 104], r14
    mov [rdi + 112], r15

    mov rax, rsp
    mov [rdi + 120], rax

    mov rax, [rsp]
    mov [rdi + 128], rax

    pushfq
    pop rax
    mov [rdi + 136], rax

.load_new:
    mov rdx, rsi

    mov rax,  [rdx + 0]
    mov rbx,  [rdx + 8]
    mov rcx,  [rdx + 16]
    mov rdx,  [rdx + 24]
    mov rbp,  [rsi + 48]
    mov r8,   [rsi + 56]
    mov r9,   [rsi + 64]
    mov r10,  [rsi + 72]
    mov r11,  [rsi + 80]
    mov r12,  [rsi + 88]
    mov r13,  [rsi + 96]
    mov r14,  [rsi + 104]
    mov r15,  [rsi + 112]

    mov rsp,  [rsi + 120]

    mov rax,  [rsi + 136]
    push rax
    popfq

    mov rax, [rsi + 128]
    push rax

    mov rdi, [rsi + 40]
    mov rsi, [rsi + 32]

    ret
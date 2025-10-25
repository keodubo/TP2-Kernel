; Trampas ASM que invocan int 0x80 con el número correcto de syscall
GLOBAL sys_read
GLOBAL sys_write
GLOBAL sys_clear
GLOBAL sys_getMinutes
GLOBAL sys_getHours
GLOBAL sys_getSeconds
GLOBAL sys_scrHeight
GLOBAL sys_scrWidth
GLOBAL sys_drawRectangle
GLOBAL sys_wait
GLOBAL sys_registerInfo
GLOBAL sys_drawCursor
GLOBAL sys_printmem
GLOBAL sys_pixelMinus
GLOBAL sys_pixelPlus
GLOBAL sys_playSpeaker
GLOBAL sys_writeColor
GLOBAL sys_stopSpeaker
GLOBAL sys_malloc
GLOBAL sys_free
GLOBAL sys_mem_info
GLOBAL sys_mm_get_stats
GLOBAL sys_getpid
GLOBAL sys_create_process
GLOBAL sys_kill
GLOBAL sys_block
GLOBAL sys_unblock
GLOBAL sys_nice
GLOBAL sys_yield
GLOBAL sys_wait_pid
GLOBAL sys_sem_open
GLOBAL sys_sem_wait
GLOBAL sys_sem_post
GLOBAL sys_sem_close
GLOBAL sys_sem_unlink
GLOBAL sys_exit
GLOBAL sys_proc_snapshot
GLOBAL sys_pipe_open
GLOBAL sys_pipe_close
GLOBAL sys_pipe_read
GLOBAL sys_pipe_write
GLOBAL sys_pipe_unlink
GLOBAL sys_read_fd
GLOBAL sys_write_fd
GLOBAL sys_close_fd
GLOBAL sys_dup2
GLOBAL sys_create_process_ex
section .text

; Pasaje de parametros en C:
; %rdi %rsi %rdx %rcx %r8 %r9

; Pasaje de parametros para sys_calls
; %rdi %rsi %rdx %r10 %r8 %r9

; MOVER RCX a R10

; %rcx y %rc11 son destruidos si las llamo desde aca

; %rax el numero de la syscall

sys_read:
    mov rax, 0x00
    int 80h
    ret

sys_write:
    mov rax, 0x01
    int 80h
    ret

sys_clear:
    mov rax, 0x02
    int 80h
    ret

sys_getHours:
    mov rax, 0x03
    int 80h
    ret

sys_getMinutes:
    mov rax, 0x04
    int 80h
    ret

sys_getSeconds:
    mov rax, 0x05
    int 80h
    ret

sys_scrHeight:
    mov rax, 0x06
    int 80h
    ret

sys_scrWidth:
    mov rax, 0x07
    int 80h
    ret

sys_drawRectangle:
    mov rax, 0x08
    mov r10, rcx        ;4to parametro de syscall es R10
    int 80h
    ret

sys_wait:
    mov rax, 0x09
    int 80h
    ret

sys_registerInfo:
    mov rax, 0x0A
    int 80h
    ret

sys_printmem: 
    mov rax, 0x0B
    int 80h
    ret

sys_pixelPlus: 
    mov rax, 0x0C
    int 80h
    ret

sys_pixelMinus: 
    mov rax, 0x0D
    int 80h
    ret

sys_playSpeaker: 
    mov rax, 0x0E
    int 80h
    ret

sys_stopSpeaker: 
    mov rax, 0x0F
    int 80h
    ret

sys_drawCursor:
    mov rax, 0x10
    int 80h
    ret

sys_writeColor:
    mov rax, 0x11
    int 80h
    ret

sys_malloc:
    mov rax, 0x12
    int 80h
    ret

sys_free:
    mov rax, 0x13
    int 80h
    ret

sys_mem_info:
    mov rax, 0x14
    int 80h
    ret

sys_mm_get_stats:
    mov rax, 0x2E
    int 80h
    ret

sys_getpid:
    mov rax, 0x15
    int 80h
    ret

sys_create_process:
    mov rax, 0x16
    mov r10, rcx        ;4to parametro de syscall es R10
    int 80h
    ret

sys_kill:
    mov rax, 0x17
    int 80h
    ret

sys_block:
    mov rax, 0x18
    int 80h
    ret

sys_unblock:
    mov rax, 0x19
    int 80h
    ret

sys_nice:
    mov rax, 0x1A
    int 80h
    ret

sys_yield:
    mov rax, 0x1B
    int 80h
    ret

sys_wait_pid:
    mov rax, 0x1C
    int 80h
    ret

sys_exit:
    mov rax, 0x1D
    int 80h
    ret

sys_proc_snapshot:
    mov rax, 0x1E
    int 80h
    ret

sys_sem_open:
    mov rax, 0x1F
    int 80h
    ret

sys_sem_wait:
    mov rax, 0x20
    int 80h
    ret

sys_sem_post:
    mov rax, 0x21
    int 80h
    ret

sys_sem_close:
    mov rax, 0x22
    int 80h
    ret

sys_sem_unlink:
    mov rax, 0x23
    int 80h
    ret

; Pipes (Hito 5)
sys_pipe_open:
    mov rax, 36
    int 80h
    ret

sys_pipe_close:
    mov rax, 37
    int 80h
    ret

sys_pipe_read:
    mov rax, 38
    int 80h
    ret

sys_pipe_write:
    mov rax, 39
    int 80h
    ret

sys_pipe_unlink:
    mov rax, 40
    int 80h
    ret

; FD genéricos
sys_read_fd:
    mov rax, 41
    int 80h
    ret

sys_write_fd:
    mov rax, 42
    int 80h
    ret

sys_close_fd:
    mov rax, 43
    int 80h
    ret

sys_dup2:
    mov rax, 44
    int 80h
    ret

sys_create_process_ex:
    mov rax, 45
    mov r10, rcx        ; 4to parámetro (name)
    ; r8 ya tiene el 5to parámetro (priority)
    ; r9 ya tiene el 6to parámetro (is_fg)
    int 80h
    ret

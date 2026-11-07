[BITS 64]
global cpu_context_switch
global task_trampoline_asm
extern task_exit

; void cpu_context_switch(uint64_t* old_rsp [RDI], uint64_t new_rsp [RSI])
cpu_context_switch:
    pushfq
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov [rdi], rsp
    mov rsp, rsi

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    popfq

    ret

; Трамплин для безопасного запуска задачи
; Ожидает: R14 = entry_point, R13 = arg (syscall_dispatcher)
task_trampoline_asm:
    mov rdi, r13              ; Передаем dispatcher_ptr в RDI (первый аргумент)
    call r14                  ; Вызываем entry_point(_start)
    call task_exit            ; Если бинарник сделал ret, завершаем таск
.halt_loop:
    hlt
    jmp .halt_loop

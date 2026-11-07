[BITS 64]
[DEFAULT REL]

global _start

_start:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Сохраняем адрес диспетчера ядра в R12
    mov r12, rdi

main_loop:
    ; 1. Вывод приглашения "Enter Num1: "
    lea rdx, [rel msg_num1]
    mov rcx, len_num1
    call print_str

    ; Считываем первое число -> R13
    call read_number
    mov r13, rax

    ; 2. Вывод приглашения "Enter Op (+, -, *, /): "
    lea rdx, [rel msg_op]
    mov rcx, len_op
    call print_str

    ; Считываем оператор -> R14
    call read_char_single
    mov r14, rax

    ; Выводим перенос строки после ввода оператора
    lea rdx, [rel msg_newline]
    mov rcx, 1
    call print_str

    ; 3. Вывод приглашения "Enter Num2: "
    lea rdx, [rel msg_num2]
    mov rcx, len_num2
    call print_str

    ; Считываем второе число -> R15
    call read_number
    mov r15, rax

    ; 4. Выполнение вычислений (R13 op R15)
    cmp r14b, '+'
    je do_add
    cmp r14b, '-'
    je do_sub
    cmp r14b, '*'
    je do_mul
    cmp r14b, '/'
    je do_div

    ; Неизвестный оператор
    lea rdx, [rel msg_err_op]
    mov rcx, len_err_op
    call print_str
    jmp main_loop

do_add:
    mov rax, r13
    add rax, r15
    jmp print_result

do_sub:
    mov rax, r13
    sub rax, r15
    jmp print_result

do_mul:
    mov rax, r13
    imul rax, r15
    jmp print_result

do_div:
    cmp r15, 0
    je div_by_zero
    mov rax, r13
    cqo                        ; Расширяем RAX до RDX:RAX для idiv
    idiv r15
    jmp print_result

div_by_zero:
    lea rdx, [rel msg_err_div]
    mov rcx, len_err_div
    call print_str
    jmp main_loop

print_result:
    push rax

    ; Выводим "Result: "
    lea rdx, [rel msg_res]
    mov rcx, len_res
    call print_str

    pop rax
    call print_number

    lea rdx, [rel msg_newline]
    mov rcx, 1
    call print_str

    jmp main_loop

exit_app:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; =========================================================================
; Вспомогательные функции взаимодействия с ядром
; =========================================================================

; Безопасный системный вызов с выравниванием стека по 16 байт
do_syscall:
    push rbp
    mov rbp, rsp
    and rsp, -16
    call r12
    mov rsp, rbp
    pop rbp
    ret

; Вывод строки: RDX = адрес, RCX = длина
print_str:
    mov rdi, 1                 ; SYS_WRITE
    mov rsi, 0
    call do_syscall
    ret

; Считывание одного символа с ожиданием (символ в RAX)
read_char_single:
.wait:
    mov rdi, 2                 ; SYS_READ_CHAR
    mov rsi, 0
    mov rdx, 0
    mov rcx, 0
    call do_syscall
    cmp al, 0
    je .wait

    ; Эхо-печать введенного символа
    mov [rel char_tmp], al
    push rax
    mov rdi, 1                 ; SYS_WRITE
    mov rsi, 0
    lea rdx, [rel char_tmp]
    mov rcx, 1
    call do_syscall
    pop rax
    ret

; Считывание целого числа (R15 используется как безопасный Callee-Saved счетчик)
read_number:
    push rbx
    push r15                   ; Сохраняем R15, чтобы не затереть Num2 из main_loop

    lea rbx, [rel buffer]
    mov r15, 0                 ; Длина строки в R15

.read_loop:
    call read_char_single

    cmp al, 0x0A               ; Enter
    je .done

    cmp al, 0x08               ; Backspace
    je .handle_bs

    cmp r15, 31
    jge .read_loop

    mov [rbx + r15], al
    inc r15
    jmp .read_loop

.handle_bs:
    cmp r15, 0
    jle .read_loop
    dec r15
    mov byte [rbx + r15], 0
    jmp .read_loop

.done:
    mov byte [rbx + r15], 0

    lea rdx, [rel msg_newline]
    mov rcx, 1
    call print_str

    lea rsi, [rel buffer]
    call parse_int

    pop r15
    pop rbx
    ret

; Преобразование строки (RSI) в число (RAX)
parse_int:
    xor rax, rax
    xor rcx, rcx               ; Флаг знака (0 = +, 1 = -)
    xor r8, r8

    mov r8b, [rsi]
    cmp r8b, '-'
    jne .loop
    inc rsi
    mov rcx, 1

.loop:
    mov r8b, [rsi]
    inc rsi
    cmp r8b, 0
    je .apply_sign
    cmp r8b, '0'
    jl .apply_sign
    cmp r8b, '9'
    jg .apply_sign

    sub r8b, '0'
    imul rax, 10
    add rax, r8
    jmp .loop

.apply_sign:
    cmp rcx, 0
    je .finish
    neg rax

.finish:
    ret

; Вывод 64-битного целого числа из RAX
print_number:
    push rbx
    lea rbx, [rel num_buf + 31]
    mov byte [rbx], 0          ; Null-terminator

    mov r8, rax                ; Сохраняем исходное число для проверки знака
    cmp rax, 0
    jge .pos
    neg rax

.pos:
    mov rcx, 10

.convert_loop:
    xor rdx, rdx
    div rcx
    add dl, '0'
    dec rbx
    mov [rbx], dl
    test rax, rax
    jnz .convert_loop

    cmp r8, 0
    jge .print_it
    dec rbx
    mov byte [rbx], '-'

.print_it:
    lea rdx, [rel num_buf + 31]
    sub rdx, rbx               ; RDX = длина
    mov rcx, rdx
    mov rdx, rbx               ; RDX = адрес

    call print_str

    pop rbx
    ret

; =========================================================================
; Данные
; =========================================================================

msg_num1:    db "Enter Num1: ", 0
len_num1     equ $ - msg_num1

msg_op:      db "Enter Op (+, -, *, /): ", 0
len_op       equ $ - msg_op

msg_num2:    db "Enter Num2: ", 0
len_num2     equ $ - msg_num2

msg_res:     db "Result: ", 0
len_res      equ $ - msg_res

msg_err_op:  db 0x0A, "Error: Unknown operator!", 0x0A, 0
len_err_op   equ $ - msg_err_op

msg_err_div: db "Error: Division by zero!", 0x0A, 0
len_err_div  equ $ - msg_err_div

msg_newline: db 0x0A, 0

char_tmp:    db 0
buffer:      times 32 db 0
num_buf:     times 32 db 0

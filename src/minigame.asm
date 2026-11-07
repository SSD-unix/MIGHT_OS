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

    ; Адрес диспетчера ядра в R12
    mov r12, rdi

    ; 1. Генерируем случайное число 1..100 -> сохраняем в R13
    call get_random_1_100
    mov r13, rax

    ; Инициализация счетчика попыток -> R14
    mov r14, 0

    ; Приветствие
    lea rdx, [msg_welcome]
    mov rcx, len_welcome
    call print_str

game_loop:
    inc r14                    ; Инкремент попыток

    ; Приглашение ввода "Guess (1-100) > "
    lea rdx, [msg_prompt]
    mov rcx, len_prompt
    call print_str

    ; Считываем число от пользователя
    call read_number
    mov rbx, rax                ; Введенное число в RBX

    ; Сравниваем с загаданным (R13)
    cmp rbx, r13
    je win_game
    jl guess_higher

guess_lower:
    lea rdx, [msg_lower]
    mov rcx, len_lower
    call print_str
    jmp game_loop

guess_higher:
    lea rdx, [msg_higher]
    mov rcx, len_higher
    call print_str
    jmp game_loop

win_game:
    lea rdx, [msg_win]
    mov rcx, len_win
    call print_str

    ; Печатаем количество попыток
    mov rax, r14
    call print_number

    lea rdx, [msg_tries]
    mov rcx, len_tries
    call print_str

exit_app:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; =========================================================================
; ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
; =========================================================================

; Печать строки: RDX = адрес, RCX = длина
print_str:
    mov rdi, 1                 ; SYS_WRITE
    mov rsi, 0
    call r12
    ret

; Считывание целого числа с поддержкой Backspace и Enter
read_number:
    push rbx
    push r11

    lea rbx, [buffer]
    mov r11, 0                 ; Длина ввода

.read_loop:
    mov rdi, 2                 ; SYS_READ_CHAR
    mov rsi, 0
    mov rdx, 0
    mov rcx, 0
    call r12                   ; Символ в RAX

    cmp al, 0
    je .read_loop

    cmp al, 0x0A               ; Enter
    je .done

    cmp al, 0x08               ; Backspace
    je .handle_bs

    cmp r11, 15
    jge .read_loop

    ; Сохраняем и делаем эхо на экран
    mov [rbx + r11], al
    inc r11

    mov [char_tmp], al
    lea rdx, [char_tmp]
    mov rcx, 1
    call print_str
    jmp .read_loop

.handle_bs:
    cmp r11, 0
    jle .read_loop
    dec r11
    mov byte [rbx + r11], 0

    mov byte [char_tmp], 0x08
    lea rdx, [char_tmp]
    mov rcx, 1
    call print_str
    jmp .read_loop

.done:
    mov byte [rbx + r11], 0

    ; Перевод строки
    lea rdx, [msg_newline]
    mov rcx, 1
    call print_str

    ; Парсим число из строки buffer
    lea rsi, [buffer]
    call parse_int

    pop r11
    pop rbx
    ret

; Перевод ASCII строки в RAX
parse_int:
    xor rax, rax
    xor r8, r8

.loop:
    mov r8b, [rsi]
    inc rsi
    cmp r8b, '0'
    jl .finish
    cmp r8b, '9'
    jg .finish

    sub r8b, '0'
    imul rax, 10
    add rax, r8
    jmp .loop

.finish:
    ret

; Вывод числа из RAX
print_number:
    push rbx
    lea rbx, [num_buf + 31]
    mov byte [rbx], 0

    mov rcx, 10
.convert:
    xor rdx, rdx
    div rcx
    add dl, '0'
    dec rbx
    mov [rbx], dl
    test rax, rax
    jnz .convert

    lea rdx, [num_buf + 31]
    sub rdx, rbx
    mov rcx, rdx
    mov rdx, rbx
    call print_str
    pop rbx
    ret

; Генерация числа 1..100 через RDTSC
get_random_1_100:
    rdtsc
    xor rax, rdx
    mov rcx, 100
    xor rdx, rdx
    div rcx                    ; Остаток в RDX (0..99)
    inc rdx                    ; (1..100)
    mov rax, rdx
    ret

; =========================================================================
; ДАННЫЕ
; =========================================================================

msg_welcome: db "=== GUESS THE NUMBER (1 - 100) ===", 0x0A, 0
len_welcome  equ $ - msg_welcome

msg_prompt:  db "Guess > ", 0
len_prompt   equ $ - msg_prompt

msg_higher:  db "Higher! Try again.", 0x0A, 0
len_higher   equ $ - msg_higher

msg_lower:   db "Lower! Try again.", 0x0A, 0
len_lower    equ $ - msg_lower

msg_win:     db 0x0A, "YOU WON! Total attempts: ", 0
len_win      equ $ - msg_win

msg_tries:   db 0x0A, 0
len_tries    equ $ - msg_tries

msg_newline: db 0x0A, 0

char_tmp:    db 0
buffer:      times 32 db 0
num_buf:     times 32 db 0

[BITS 64]
[DEFAULT REL]

global _start

_start:
    push rbp
    mov rbp, rsp
    push rbx
    push r12

    ; Адрес диспетчера ядра в R12
    mov r12, rdi

    ; 1. Приглашение
    mov rdi, 1                  ; SYS_WRITE
    mov rsi, 0
    lea rdx, [msg_prompt]
    mov rcx, len_prompt
    call r12

    lea rbx, [buffer]
    mov r11, 0                  ; Длина вводимой строки

read_loop:
    mov rdi, 2                  ; SYS_READ_CHAR
    mov rsi, 0
    mov rdx, 0
    mov rcx, 0
    call r12                    ; Ядро возвращает готовую ASCII букву в RAX

    cmp al, 0
    je read_loop                ; Пропускаем мусор/break-коды

    ; Проверка на Enter ('\n' = 0x0A)
    cmp al, 0x0A
    je input_done

    ; Проверка на Backspace ('\b' = 0x08)
    cmp al, 0x08
    je handle_backspace

    ; Защита от переполнения
    cmp r11, 127
    jge read_loop

    ; Сохраняем и печатаем символ
    mov [rbx + r11], al
    inc r11

    mov [char_tmp], al
    mov rdi, 1                  ; SYS_WRITE
    mov rsi, 0
    lea rdx, [char_tmp]
    mov rcx, 1
    call r12

    jmp read_loop

handle_backspace:
    cmp r11, 0
    jle read_loop

    dec r11
    mov byte [rbx + r11], 0

    ; Отправляем '\b' в SYS_WRITE (ядро само сотрет символ на экране)
    mov byte [char_tmp], 0x08
    mov rdi, 1
    mov rsi, 0
    lea rdx, [char_tmp]
    mov rcx, 1
    call r12

    jmp read_loop

input_done:
    mov byte [rbx + r11], 0

    ; Печать "You said: "
    mov rdi, 1
    mov rsi, 0
    lea rdx, [msg_reply]
    mov rcx, len_reply
    call r12

    ; Печать текста
    cmp r11, 0
    je exit_app

    mov rdi, 1
    mov rsi, 0
    lea rdx, [buffer]
    mov rcx, r11
    call r12

    mov rdi, 1
    mov rsi, 0
    lea rdx, [msg_newline]
    mov rcx, 1
    call r12

exit_app:
    pop r12
    pop rbx
    pop rbp
    ret

msg_prompt:  db "Echo > ", 0
len_prompt   equ $ - msg_prompt

msg_reply:   db 0x0A, "You said: ", 0
len_reply    equ $ - msg_reply

msg_newline: db 0x0A, 0

char_tmp:    db 0
buffer:      times 128 db 0

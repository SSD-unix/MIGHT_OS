[BITS 64]
[DEFAULT REL]

global _start

_start:
    push rbp
    mov rbp, rsp
    push rbx
    push r12

    ; 1. Сохраняем адрес диспетчера syscall_dispatcher из RDI в R12
    mov r12, rdi

    ; 2. Создаем графическое окно браузера (SYS_CREATE_WINDOW = 10)
    ; win_id = wm_create_window(x=100, y=80, w=540, h=320, title="MIGHT Web Browser")
    mov rdi, 10                  ; SYS_CREATE_WINDOW
    lea rsi, [win_title]         ; a1: Заголовок окна
    mov rdx, 540                 ; a2: Ширина (w)
    mov rcx, 320                 ; a3: Высота (h)
    mov r8, 100                  ; a4: X на экране
    mov r9, 80                   ; a5: Y на экране
    call r12
    mov rbx, rax                 ; Сохраняем win_id в RBX

    ; 3. Рисуем кнопку "FETCH HTTP" (SYS_DRAW_BUTTON = 13)
    ; a5 упакован: (bg_color << 32) | (height << 16) | width
    ; 0x3B82F6 (Синий цвет) | h=26 (0x001A) | w=110 (0x006E) -> 0x3B82F6001A006E
    mov rdi, 13                  ; SYS_DRAW_BUTTON
    mov rsi, rbx                 ; a1: win_id
    mov rdx, 15                  ; a2: rel_x в окне
    mov rcx, 15                  ; a3: rel_y в окне
    lea r8, [btn_label]          ; a4: Текст кнопки
    mov r9, 0x3B82F6001A006E     ; a5: Упакованный размер и цвет
    call r12

    ; 4. Рисуем статусное сообщение (SYS_DRAW_TEXT = 12)
    mov rdi, 12                  ; SYS_DRAW_TEXT
    mov rsi, rbx                 ; a1: win_id
    mov rdx, 135                 ; a2: rel_x
    mov rcx, 20                  ; a3: rel_y
    lea r8, [msg_status]         ; a4: Текст
    mov r9, 0x00FF00             ; a5: Цвет (Зеленый)
    call r12

    ; 5. Вызываем сетевой системный вызов (SYS_HTTP_GET = 20)
    ; Ядро устанавливает TCP Handshake с 10.0.2.2:80 и качает "/"
    mov rdi, 20                  ; SYS_HTTP_GET
    lea rsi, [target_ip]         ; a1: IP "10.0.2.2"
    lea rdx, [target_path]       ; a2: Path "/"
    call r12

    ; 6. Принудительно обновляем графический кадр (SYS_REDRAW_WIN = 15)
    mov rdi, 15                  ; SYS_REDRAW_WIN
    call r12

event_loop:
    ; 7. Опрашиваем события GUI-окна (SYS_POLL_EVENT = 14)
    mov rdi, 14                  ; SYS_POLL_EVENT
    mov rsi, rbx                 ; a1: win_id
    lea rdx, [event_struct]      ; a2: Указатель на структуру gui_event
    call r12

    ; Перерисовываем экран
    mov rdi, 15                  ; SYS_REDRAW_WIN
    call r12

    jmp event_loop

exit_app:
    ; Выход из приложения (SYS_EXIT = 60)
    mov rdi, 60                  ; SYS_EXIT
    call r12

    pop r12
    pop rbx
    pop rbp
    ret

; === Секция данных ===
win_title:    db "MIGHT Web Browser", 0
btn_label:    db "FETCH HTTP", 0
msg_status:   db "HTTP GET 10.0.2.2 / Sent!", 0

target_ip:    db "10.0.2.2", 0
target_path:  db "/", 0

align 8
event_struct: times 32 db 0

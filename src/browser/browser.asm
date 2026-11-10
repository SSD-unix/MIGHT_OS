; ==============================================================================
; MIGHT OS 1.1 - Native Assembly Web Browser & HTML/CSS Renderer
; Target: x86-64 Flat Binary (System V ABI Aligned)
; ==============================================================================

[BITS 64]
[DEFAULT REL]

global _start

; --- Системные вызовы MIGHT OS Kernel ---
%define SYS_CREATE_WINDOW 10
%define SYS_DRAW_RECT     11
%define SYS_DRAW_TEXT     12
%define SYS_DRAW_BUTTON   13
%define SYS_POLL_EVENT    14
%define SYS_REDRAW_WIN    15
%define SYS_HTTP_GET      20
%define SYS_EXIT          60

; --- Цветовая палитра GUI ---
%define COLOR_BG          0x1E222A
%define COLOR_CANVAS      0x21252B
%define COLOR_NAVBAR      0x181B28
%define COLOR_TEXT_DEF    0xABB2BF
%define COLOR_H1          0x61AFEF
%define COLOR_H2          0xE5C07B
%define COLOR_LINK        0x98C379
%define COLOR_LINE        0x3E4451
%define COLOR_BTN_BG      0x3B82F6

section .text

_start:
    ; 1. Выравнивание стека по 16 байт (4 push = 32 байта)
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13

    mov r12, rdi             ; Сохраняем syscall_dispatcher ядра

    ; 2. Создание окна приложения
    mov rdi, SYS_CREATE_WINDOW
    lea rsi, [win_title]
    mov rdx, 660             ; w
    mov rcx, 460             ; h
    mov r8, 50               ; x
    mov r9, 40               ; y
    call r12
    mov rbx, rax             ; win_id -> RBX

    ; 3. Отрисовка верхушки браузера (Navbar, URL Bar)
    call draw_browser_gui

    ; 4. Вызов HTTP GET (передаем адрес net_buffer в RCX/a3)
    mov rdi, SYS_HTTP_GET
    lea rsi, [target_ip]
    lea rdx, [target_path]
    lea rcx, [net_buffer]    ; a3 = net_buffer
    xor r8, r8
    xor r9, r9
    call r12

    ; 5. Проверяем пришедший буфер: если в net_buffer есть данные, рендерим их
    cmp byte [net_buffer], 0
    jne .render_network

    ; Если буфер пуст, рендерим fallback
    lea rsi, [sample_html]
    call render_html_page
    jmp .done_init

.render_network:
    lea rsi, [net_buffer]
    call render_html_page

.done_init:
    call force_redraw

event_loop:
    mov rdi, SYS_POLL_EVENT
    mov rsi, rbx
    lea rdx, [event_struct]
    xor rcx, rcx
    xor r8, r8
    xor r9, r9
    call r12

    cmp rax, 0
    jle .no_event
    mov eax, [event_struct + 4]
    cmp eax, 13
    jne .no_event

    ; Нажата кнопка FETCH
    mov rdi, SYS_HTTP_GET
    lea rsi, [target_ip]
    lea rdx, [target_path]
    lea rcx, [net_buffer]
    xor r8, r8
    xor r9, r9
    call r12

    lea rsi, [net_buffer]
    call render_html_page

.no_event:
    call force_redraw
    jmp event_loop

exit_app:
    mov rdi, SYS_EXIT
    xor rsi, rsi
    xor rdx, rdx
    xor rcx, rcx
    xor r8, r8
    xor r9, r9
    call r12

    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; ==============================================================================
; GUI NAVBAR
; ==============================================================================
draw_browser_gui:
    push rsi
    push rdx
    push rcx

    mov rdi, SYS_DRAW_RECT
    mov rsi, rbx
    mov rdx, 5
    mov rcx, 5
    mov r8, 650
    mov r9, (COLOR_NAVBAR << 32) | 35
    call r12

    mov rdi, SYS_DRAW_BUTTON
    mov rsi, rbx
    mov rdx, 10
    mov rcx, 10
    lea r8, [btn_back_text]
    mov r9, (0x4B5263 << 32) | (24 << 16) | 55
    call r12

    mov rdi, SYS_DRAW_BUTTON
    mov rsi, rbx
    mov rdx, 70
    mov rcx, 10
    lea r8, [btn_refresh_text]
    mov r9, (0x4B5263 << 32) | (24 << 16) | 70
    call r12

    mov rdi, SYS_DRAW_RECT
    mov rsi, rbx
    mov rdx, 145
    mov rcx, 10
    mov r8, 410
    mov r9, (0x111318 << 32) | 24
    call r12

    mov rdi, SYS_DRAW_TEXT
    mov rsi, rbx
    mov rdx, 155
    mov rcx, 14
    lea r8, [url_display_text]
    mov r9, 0x61AFEF
    call r12

    mov rdi, SYS_DRAW_BUTTON
    mov rsi, rbx
    mov rdx, 565
    mov rcx, 10
    lea r8, [btn_fetch_text]
    mov r9, (COLOR_BTN_BG << 32) | (24 << 16) | 80
    call r12

    pop rcx
    pop rdx
    pop rsi
    ret

; ==============================================================================
; РЕНДЕР HTML ДОКУМЕНТА (С фильтром HTTP заголовков)
; ==============================================================================
render_html_page:
    push rbp
    mov rbp, rsp
    push r12
    push rbx

    mov dword [cursor_x], 20
    mov dword [cursor_y], 55
    mov dword [curr_color], COLOR_TEXT_DEF
    mov dword [token_len], 0

    ; Очищаем холст
    mov rdi, SYS_DRAW_RECT
    mov rsi, rbx
    mov rdx, 10
    mov rcx, 45
    mov r8, 640
    mov r9, (COLOR_CANVAS << 32) | 385
    call r12

    ; 1. Пропускаем HTTP заголовки (ищем первейший тег '<')
.skip_http_header:
    mov al, [rsi]
    test al, al
    jz .done
    cmp al, '<'
    je .parse_loop
    inc rsi
    jmp .skip_http_header

.parse_loop:
    mov al, [rsi]
    test al, al
    jz .done

    cmp al, '<'
    je .handle_tag

    ; Игнорируем переносы строк
    cmp al, 0Ah
    je .skip_c
    cmp al, 0Dh
    je .skip_c

    ; Защита буфера
    mov ecx, [token_len]
    cmp ecx, 240
    jge .skip_c

    lea rdi, [token_buf]
    mov [rdi + rcx], al
    inc dword [token_len]

.skip_c:
    inc rsi
    jmp .parse_loop

.handle_tag:
    call flush_text
    inc rsi

    cmp byte [rsi], '/'
    je .close_tag

    cmp byte [rsi], 'h'
    je .check_h
    cmp byte [rsi], 'p'
    je .tag_p
    cmp byte [rsi], 'a'
    je .tag_a
    cmp byte [rsi], 'l'
    je .tag_li
    cmp byte [rsi], 'b'
    je .check_br

    jmp .skip_unknown_tag

.check_h:
    inc rsi
    cmp byte [rsi], '1'
    je .tag_h1
    cmp byte [rsi], '2'
    je .tag_h2
    cmp byte [rsi], 'r'
    je .tag_hr
    jmp .skip_unknown_tag

.check_br:
    inc rsi
    cmp byte [rsi], 'r'
    je .tag_br
    jmp .skip_unknown_tag

.tag_h1:
    add dword [cursor_y], 20
    mov dword [cursor_x], 20
    mov dword [curr_color], COLOR_H1
    jmp .skip_attr

.tag_h2:
    add dword [cursor_y], 16
    mov dword [cursor_x], 20
    mov dword [curr_color], COLOR_H2
    jmp .skip_attr

.tag_p:
    add dword [cursor_y], 14
    mov dword [cursor_x], 20
    mov dword [curr_color], COLOR_TEXT_DEF
    jmp .skip_attr

.tag_li:
    add dword [cursor_y], 14
    mov dword [cursor_x], 30
    mov dword [curr_color], COLOR_TEXT_DEF
    jmp .skip_attr

.tag_a:
    mov dword [curr_color], COLOR_LINK
    jmp .skip_attr

.tag_br:
    add dword [cursor_y], 14
    mov dword [cursor_x], 20
    jmp .skip_attr

.tag_hr:
    add dword [cursor_y], 8
    mov rdi, SYS_DRAW_RECT
    mov rsi, rbx
    mov rdx, 20
    mov ecx, [cursor_y]
    mov r8, 620
    mov r9, (COLOR_LINE << 32) | 2
    call r12
    add dword [cursor_y], 8
    mov dword [cursor_x], 20
    jmp .skip_attr

.close_tag:
    mov dword [curr_color], COLOR_TEXT_DEF

.skip_attr:
    mov al, [rsi]
    test al, al
    jz .done
    cmp al, '>'
    je .tag_end
    inc rsi
    jmp .skip_attr

.tag_end:
    inc rsi
    jmp .parse_loop

.skip_unknown_tag:
    mov al, [rsi]
    test al, al
    jz .done
    cmp al, '>'
    je .tag_end
    inc rsi
    jmp .skip_unknown_tag

.done:
    call flush_text
    pop rbx
    pop r12
    pop rbp
    ret

flush_text:
    mov ecx, [token_len]
    test ecx, ecx
    jz .ret

    lea rdi, [token_buf]
    mov byte [rdi + rcx], 0

    push rsi
    mov rdi, SYS_DRAW_TEXT
    mov rsi, rbx
    mov edx, [cursor_x]
    mov ecx, [cursor_y]
    lea r8, [token_buf]
    mov r9d, [curr_color]
    call r12
    pop rsi

    mov eax, [token_len]
    shl eax, 3
    add [cursor_x], eax

    ; Ограничиваем снос по ширине окна
    cmp dword [cursor_x], 600
    jle .no_wrap
    mov dword [cursor_x], 20
    add dword [cursor_y], 14
.no_wrap:
    mov dword [token_len], 0
.ret:
    ret

force_redraw:
    mov rdi, SYS_REDRAW_WIN
    xor rsi, rsi
    xor rdx, rdx
    xor rcx, rcx
    xor r8, r8
    xor r9, r9
    call r12
    ret

section .data

cursor_x:          dd 20
cursor_y:          dd 55
curr_color:        dd COLOR_TEXT_DEF
token_len:         dd 0

win_title:         db "MIGHT Web Browser v1.1", 0
url_display_text:  db "http://10.0.2.2/ | Directory Listing", 0

btn_back_text:     db "<", 0
btn_refresh_text:  db "R", 0
btn_fetch_text:    db "FETCH", 0

target_ip:         db "10.0.2.2", 0
target_path:       db "/", 0

sample_html:
    db "<h1>Directory listing for /</h1>", 0Ah
    db "<hr>", 0Ah
    db "<li><a href=", 22h, "#", 22h, ">.bashrc</a></li>", 0Ah
    db "<li><a href=", 22h, "#", 22h, ">Desktop/</a></li>", 0Ah
    db "<li><a href=", 22h, "#", 22h, ">Documents/</a></li>", 0Ah
    db "<li><a href=", 22h, "#", 22h, ">Downloads/</a></li>", 0

align 16
event_struct:      times 32 db 0

section .bss
token_buf:         resb 512
net_buffer:        resb 8192

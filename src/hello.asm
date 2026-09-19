[bits 64]
[org 0x2000000]     ; Точка входа в оперативке

_start:
    ; --- Syscall 1: sys_write ---
    mov rax, 1          ; Номер системного вызова sys_write
    mov rdi, 1          ; fd (stdout)
    mov rsi, msg        ; Указатель на строку
    mov rdx, msg_len    ; Длина строки
    int 0x80            ; Прерывание -> Прыжок в ядро!

    ; --- Syscall 60: sys_exit ---
    mov rax, 60         ; Номер вызова sys_exit
    mov rdi, 0          ; Код возврата 0
    int 0x80            ; Возвращаем управление

msg: db "Hello from external 64-bit binary in MIGHT OS!", 0xA, 0
msg_len: equ $ - msg

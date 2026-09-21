; kernel_entry.asm
; Multiboot2 заголовок + переход в Long Mode

section .multiboot
align 8
    dd 0xE85250D6
    dd 0x0
    dd header_end - header_start
    dd -(0xE85250D6 + 0 + (header_end - header_start))

header_start:
    align 8
    dw 0x4
    dw 0x0
    dd 0x0
    align 8
    dw 0x0
    dw 0x0
    dd 0x0
header_end:

[bits 32]
section .text
global _start
extern kmain

_start:
    cli

    ; Загружаем GDT (перед включением Long Mode)
    lgdt [gdt_pointer]

    ; Настраиваем таблицы страниц (Identity Mapping)
    mov eax, pml4
    mov cr3, eax

    ; Включаем PAE и PGE
    mov eax, cr4
    or eax, (1 << 5) | (1 << 7)
    mov cr4, eax

    ; Включаем Long Mode (EFER.LME)
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; Включаем paging (CR0.PG) и защищённый режим (уже включён)
    mov eax, cr0
    or eax, (1 << 31) | (1 << 0)
    mov cr0, eax

    ; Дальний прыжок в 64‑битный код
    jmp 0x08:long_mode_entry

[bits 64]
long_mode_entry:
    ; Обновляем сегментные регистры
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Устанавливаем стек
    mov rsp, stack_top

    ; Вызываем ядро
    call kmain
    jmp $

; ------ Таблицы страниц (Identity Map первых 4 ГБ) ------
section .data
align 4096
pml4:
    dq (pdp + 0x03)
    times 511 dq 0

align 4096
pdp:
    dq (pd + 0x03)
    times 511 dq 0

align 4096
pd:
    %assign i 0
    %rep 512
        dq (i * 0x200000 + 0x83)   ; 2MiB страницы, присутствуют, RW, исполняемые
        %assign i i+1
    %endrep

; ------ GDT ------
section .rodata
align 8
gdt:
    dq 0x0000000000000000   ; нулевой
    dq 0x00209A0000000000   ; 64‑битный код (exec/read)
    dq 0x0000920000000000   ; 64‑битные данные (read/write)
gdt_pointer:
    dw $ - gdt - 1
    dq gdt

; ------ Стек ------
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; ------ Обработчик IRQ0 (для PIT) ------
section .text
global irq0_handler
extern pit_irq_handler

irq0_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    call pit_irq_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    iretq

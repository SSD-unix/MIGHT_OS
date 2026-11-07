; ==============================================================================
; File:        boot/kernel_entry.asm
; Title:       Multiboot2 Header + Entry Point + 64-bit Long Mode Transition
; Description: Настройка 64-битного режима, таблицы страниц на 64 ГБ
;              для видеопамяти LFB и обработчик IRQ0 для PIT.
; ==============================================================================

section .multiboot
align 8
header_start:
    dd 0xE85250D6                    ; Magic number (Multiboot2)
    dd 0x0                           ; Architecture 0 (i386 protected mode)
    dd header_end - header_start     ; Header length
    dd -(0xE85250D6 + 0 + (header_end - header_start)) ; Checksum

    ; --- Tag 5: Framebuffer Request (0 = автовыбор нативного разрешения) ---
    align 8
    dw 5                             ; Type = 5 (Framebuffer)
    dw 0                             ; Flags
    dd 20                            ; Size = 20 bytes
    dd 0                             ; Width (0 = без ограничений)
    dd 0                             ; Height (0 = без ограничений)
    dd 32                            ; Depth (32 bpp ARGB)

    ; --- Tag 0: End Tag ---
    align 8
    dw 0
    dw 0
    dd 8
header_end:

[bits 32]
section .text
global _start
extern kmain

_start:
    cli

    ; Сохраняем физический адрес структур Multiboot2 (из EBX) в EDI.
    mov edi, ebx

    ; Загружаем GDT
    lgdt [gdt_pointer]

    ; Загружаем адрес PML4 в регистр управления CR3
    mov eax, pml4
    mov cr3, eax

    ; Включаем PAE (Physical Address Extension) и PGE
    mov eax, cr4
    or eax, (1 << 5) | (1 << 7)
    mov cr4, eax

    ; Включаем Long Mode в MSR (EFER.LME)
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; Включаем Paging (CR0.PG) и защищенный режим (CR0.PE)
    mov eax, cr0
    or eax, (1 << 31) | (1 << 0)
    mov cr0, eax

    ; Дальний прыжок в 64-битный сегмент кода (CODE_SEG = 0x08)
    jmp 0x08:long_mode_entry

[bits 64]
long_mode_entry:
    ; Обновляем сегментные регистры данными (DATA_SEG = 0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Устанавливаем верхушку стека ядра с выравниванием по 16 байт
    mov rsp, stack_top
    and rsp, -16

    ; Вызываем kmain(multiboot_addr)
    call kmain

    ; Бесконечный цикл, если kmain завершит работу
.loop:
    hlt
    jmp .loop

; ==============================================================================
; Обработчик IRQ0 (Таймер PIT)
; ==============================================================================
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

    ; Выравнивание стека по 16 байт перед вызовом C-функции
    mov rbp, rsp
    and rsp, -16

    call pit_irq_handler

    mov rsp, rbp

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

; ==============================================================================
; Таблицы страниц (Identity Mapping первых 64 ГБ физической памяти)
; 64 PD таблицы x 512 страниц x 2 МБ = 65 536 МБ (64 ГБ)
; Покроет LFB видеопамять по любому физическому адресу в MMIO!
; ==============================================================================
section .data
align 4096
pml4:
    dq (pdp + 0x03)             ; RW, Present
    times 511 dq 0

align 4096
pdp:
    %assign j 0
    %rep 64
        dq (pd_tables + j * 4096 + 0x03) ; 64 записи PDP -> 64 ГБ
        %assign j j+1
    %endrep
    times (512 - 64) dq 0

align 4096
pd_tables:
    %assign i 0
    %rep (64 * 512)
        dq (i * 0x200000 + 0x83) ; 2MiB страницы, RW, Present, Huge
        %assign i i+1
    %endrep

; ==============================================================================
; Глобальная таблица дескрипторов (GDT)
; ==============================================================================
section .rodata
align 8
gdt:
    dq 0x0000000000000000       ; Нулевой дескриптор
    dq 0x00209A0000000000       ; 64-битный сегмент кода (0x08)
    dq 0x0000920000000000       ; 64-битный сегмент данных (0x10)
gdt_pointer:
    dw $ - gdt - 1
    dq gdt

; ==============================================================================
; Стек ядра (64 КБ)
; ==============================================================================
section .bss
align 16
stack_bottom:
    resb 65536                  ; 64 КБ стека
stack_top:

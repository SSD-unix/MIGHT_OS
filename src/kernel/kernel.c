/* kernel/kernel.c */
#include "../common.h"
#include "../drivers/screen.h"
#include "../drivers/lowlevel_io.h"
#include "../drivers/pit.h"
#include "../drivers/idt.h"
#include "../drivers/e1000.h"
#include "utils.h"

// --- Прототипы функций ядра (для предотвращения ошибок implicit declaration) ---

void force_reset_terminal();
void kprint_int(int n);
int get_ram_size_mb();
void draw_top_clock();
void fill_rect(int col, int row, int width, int height, unsigned char color);
void print_at(const char* str, int col, int row, unsigned char color);
void draw_window(int col, int row, int width, int height, unsigned char color);
void nano_redraw(char* buf, int len, unsigned char bg_color);
void nano_mode(char* filename);
void ux_mode();

char saved_password[64] = {0};

char scancode_to_char[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
    0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

char scancode_to_char_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
    0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

// --- Подключение функций из файловой системы (fs.c) и диска ---
extern void list_files();
extern void format_disk();
extern void touch_file(char* filename);
extern void write_file(char* filename, char* data);
extern void read_file(char* filename, char* buffer);
extern void read_sector(uint32_t lba, uint8_t* buffer);

// --- Syscall Dispatcher ---
void syscall_dispatcher(uint64_t rax, uint64_t rdi, uint64_t rsi, uint64_t rdx) {
    if (rax == 1) { // sys_write
        char* buf = (char*)rsi;
        int count = (int)rdx;
        for (int i = 0; i < count; i++) {
            char str[2] = {buf[i], '\0'};
            kprint(str);
        }
    } else if (rax == 60) { // sys_exit
        kprint("\n[App Exited]\n");
    } else {
        kprint("Unknown Syscall!\n");
    }
}

// Выделяем буфер в исполняемой секции .text
static uint8_t app_executable_buffer[512] __attribute__((section(".text"), aligned(4096)));

void execute_external_app(char* filename) {
    if (!filename || filename[0] == '\0') {
        kprint("Error: No filename specified.\n");
        return;
    }

    kprint("Preparing to load: ");
    kprint(filename);
    kprint("\n");

    // 1. Очищаем буфер
    for (int i = 0; i < 512; i++) {
        app_executable_buffer[i] = 0;
    }

    // 2. Читаем СЕКТОР 31 с диска (где лежит HELLO.bin из pack.py)
    read_sector(31, app_executable_buffer);

    // Если первый байт 0 — сектор пуст
    if (app_executable_buffer[0] == 0) {
        kprint("Error: Sector 31 is empty or disk read failed.\n");
        return;
    }

    kprint("[Loader] Executing sector 31 binary...\n");
    kprint("--------------------------------------------------\n");

    // 3. Запускаем бинарник из памяти
    typedef void (*app_func_t)(void);
    app_func_t entry = (app_func_t)(uintptr_t)app_executable_buffer;

    entry();

    kprint("\n--------------------------------------------------\n");
    kprint("[Loader] Application finished successfully.\n");
}

void force_reset_terminal() {
    unsigned char *vidmem = (unsigned char*)0xB8000;
    for (int i = 0; i < 2000; i++) {
        vidmem[i * 2] = ' ';
        vidmem[i * 2 + 1] = 0x0F;
    }
    clear_screen();
}

void kprint_int(int n) {
    if (n == 0) { kprint("0"); return; }
    char s[12]; int i = 0;
    while (n > 0) { s[i++] = (n % 10) + '0'; n /= 10; }
    s[i] = '\0';
    for (int j = 0; j < i / 2; j++) { char temp = s[j]; s[j] = s[i - j - 1]; s[i - j - 1] = temp; }
    kprint(s);
}

int get_ram_size_mb() {
    unsigned char low, high;
    port_byte_out(0x70, 0x30); low = port_byte_in(0x71);
    port_byte_out(0x70, 0x31); high = port_byte_in(0x71);
    return (((high << 8) | low) + 1024) / 1024;
}

static uint32_t last_seconds = 0;

void draw_top_clock() {
    uint32_t ticks = pit_get_ticks();
    uint32_t seconds = ticks / 1000;
    if (seconds == last_seconds) return;
    last_seconds = seconds;

    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;

    unsigned char *vidmem = (unsigned char*)0xB8000;
    int offset = 72 * 2;
    unsigned char color = 0x0A;
    vidmem[offset] = (hours / 10) + '0'; vidmem[offset+1] = color;
    vidmem[offset+2] = (hours % 10) + '0'; vidmem[offset+3] = color;
    vidmem[offset+4] = ':'; vidmem[offset+5] = color;
    vidmem[offset+6] = (minutes / 10) + '0'; vidmem[offset+7] = color;
    vidmem[offset+8] = (minutes % 10) + '0'; vidmem[offset+9] = color;
    vidmem[offset+10] = ':'; vidmem[offset+11] = color;
    vidmem[offset+12] = (seconds / 10) + '0'; vidmem[offset+13] = color;
    vidmem[offset+14] = (seconds % 10) + '0'; vidmem[offset+15] = color;
}

void fill_rect(int col, int row, int width, int height, unsigned char color) {
    unsigned char *vidmem = (unsigned char*)0xB8000;
    for (int r = row; r < row + height; r++) {
        for (int c = col; c < col + width; c++) {
            vidmem[(r * 80 + c) * 2] = ' ';
            vidmem[(r * 80 + c) * 2 + 1] = color;
        }
    }
}

void print_at(const char* str, int col, int row, unsigned char color) {
    unsigned char *vidmem = (unsigned char*)0xB8000;
    int offset = (row * 80 + col) * 2;
    int i = 0;
    while (str[i] != '\0') {
        vidmem[offset] = str[i];
        vidmem[offset + 1] = color;
        offset += 2;
        i++;
    }
}

void draw_window(int col, int row, int width, int height, unsigned char color) {
    unsigned char *vidmem = (unsigned char*)0xB8000;
    unsigned char top_left = 0xC9, top_right = 0xBB, bottom_left = 0xC8, bottom_right = 0xBC;
    unsigned char horizontal = 0xCD, vertical = 0xBA;
    for (int i = 1; i < width - 1; i++) {
        vidmem[2 * (row * 80 + (col + i))] = horizontal;
        vidmem[2 * (row * 80 + (col + i)) + 1] = color;
        vidmem[2 * ((row + height - 1) * 80 + (col + i))] = horizontal;
        vidmem[2 * ((row + height - 1) * 80 + (col + i)) + 1] = color;
    }
    for (int i = 1; i < height - 1; i++) {
        vidmem[2 * ((row + i) * 80 + col)] = vertical;
        vidmem[2 * ((row + i) * 80 + col) + 1] = color;
        vidmem[2 * ((row + i) * 80 + (col + width - 1))] = vertical;
        vidmem[2 * ((row + i) * 80 + (col + width - 1)) + 1] = color;
    }
    vidmem[2 * (row * 80 + col)] = top_left;
    vidmem[2 * (row * 80 + col) + 1] = color;
    vidmem[2 * (row * 80 + (col + width - 1))] = top_right;
    vidmem[2 * (row * 80 + (col + width - 1)) + 1] = color;
    vidmem[2 * ((row + height - 1) * 80 + col)] = bottom_left;
    vidmem[2 * ((row + height - 1) * 80 + col) + 1] = color;
    vidmem[2 * ((row + height - 1) * 80 + (col + width - 1))] = bottom_right;
    vidmem[2 * ((row + height - 1) * 80 + (col + width - 1)) + 1] = color;
}

// Вспомогательная функция для обновления экрана редактора
void nano_redraw(char* buf, int len, unsigned char bg_color) {
    fill_rect(0, 1, 80, 23, bg_color);
    int x = 0, y = 1;
    unsigned char *vidmem = (unsigned char*)0xB8000;

    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            x = 0;
            if (y < 23) y++;
        } else {
            vidmem[(y * 80 + x) * 2] = buf[i];
            vidmem[(y * 80 + x) * 2 + 1] = bg_color;
            x++;
            if (x >= 80) { x = 0; if (y < 23) y++; }
        }
    }
    if (y <= 23) {
        vidmem[(y * 80 + x) * 2] = '_';
    }
}

// --- Текстовый редактор nano ---
void nano_mode(char* filename) {
    force_reset_terminal();
    unsigned char bg_color = 0x1F;
    unsigned char top_bottom_color = 0x70;

    char text_buffer[2000];
    int buf_idx = 0;
    text_buffer[0] = '\0';

    if (filename && filename[0] != '\0') {
        read_file(filename, text_buffer);
        while (text_buffer[buf_idx] != '\0' && buf_idx < 1999) {
            buf_idx++;
        }
    }

    fill_rect(0, 0, 80, 25, bg_color);
    fill_rect(0, 0, 80, 1, top_bottom_color);
    fill_rect(0, 24, 80, 1, top_bottom_color);

    print_at(" MIGHT nano 1.0 ", 2, 0, top_bottom_color);
    if (filename && filename[0] != '\0') {
        print_at("File: ", 32, 0, top_bottom_color);
        print_at(filename, 38, 0, top_bottom_color);
    } else {
        print_at("New Buffer", 35, 0, top_bottom_color);
    }

    print_at(" [F2] Save    [ESC] Exit ", 2, 24, top_bottom_color);

    int shift_pressed = 0;
    nano_redraw(text_buffer, buf_idx, bg_color);

    while (1) {
        if (port_byte_in(0x64) & 0x01) {
            unsigned char scancode = port_byte_in(0x60);

            if (scancode == 0x01) break; // Выход по ESC

            if (scancode == 0x3C) { // F2 - Сохранение
                if (filename && filename[0] != '\0') {
                    text_buffer[buf_idx] = '\0';
                    write_file(filename, text_buffer);
                    print_at(" [ SAVED ] ", 60, 0, top_bottom_color);
                } else {
                    print_at(" [ NO FILE ] ", 60, 0, top_bottom_color);
                }
                continue;
            }

            if (scancode == 0xE0) {
                while ((port_byte_in(0x64) & 0x01) == 0);
                port_byte_in(0x60);
                continue;
            }
            if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; continue; }
            if (scancode == (0x2A | 0x80) || scancode == (0x36 | 0x80)) { shift_pressed = 0; continue; }
            if (scancode & 0x80) continue;
            if (scancode == 0) continue;

            if (scancode == 0x1C) { // Enter
                if (buf_idx < 1999) {
                    text_buffer[buf_idx++] = '\n';
                    nano_redraw(text_buffer, buf_idx, bg_color);
                    print_at("            ", 60, 0, top_bottom_color);
                }
                continue;
            }
            if (scancode == 0x0E) { // Backspace
                if (buf_idx > 0) {
                    buf_idx--;
                    nano_redraw(text_buffer, buf_idx, bg_color);
                    print_at("            ", 60, 0, top_bottom_color);
                }
                continue;
            }

            if (scancode < 0x80) {
                char letter = shift_pressed ? scancode_to_char_shift[(int)scancode] : scancode_to_char[(int)scancode];
                if (letter != 0 && buf_idx < 1999) {
                    text_buffer[buf_idx++] = letter;
                    nano_redraw(text_buffer, buf_idx, bg_color);
                    print_at("            ", 60, 0, top_bottom_color);
                }
            }
        }
    }
    force_reset_terminal();
}

void ux_mode() {
    force_reset_terminal();
    unsigned char bios_color = 0x1F, select_color = 0x70;
    int selected_item = 0, total_items = 5, needs_redraw = 1;

    while (1) {
        draw_top_clock();
        if (needs_redraw) {
            fill_rect(0, 0, 80, 25, bios_color);
            draw_window(0, 0, 80, 25, bios_color);
            print_at(" MIGHT OS SETUP UTILITY ", 24, 0, bios_color);
            draw_window(2, 2, 20, 20, bios_color);
            print_at(" Main ", 4, 2, bios_color);

            print_at(selected_item == 0 ? "> System Info" : "  System Info", 4, 4, selected_item == 0 ? select_color : bios_color);
            print_at(selected_item == 1 ? "> Advanced   " : "  Advanced   ", 4, 5, selected_item == 1 ? select_color : bios_color);
            print_at(selected_item == 2 ? "> Security   " : "  Security   ", 4, 6, selected_item == 2 ? select_color : bios_color);
            print_at(selected_item == 3 ? "> Boot       " : "  Boot       ", 4, 7, selected_item == 3 ? select_color : bios_color);
            print_at(selected_item == 4 ? "> Exit       " : "  Exit       ", 4, 8, selected_item == 4 ? select_color : bios_color);

            draw_window(24, 2, 53, 20, bios_color);
            if (selected_item == 0) {
                print_at(" [ System Information ] ", 40, 2, bios_color);
                print_at("Shows kernel version and RAM size.", 26, 4, bios_color);
                int ram = get_ram_size_mb();
                char s[12]; int idx = 0;
                if (ram == 0) { s[idx++] = '0'; }
                while (ram > 0) { s[idx++] = (ram % 10) + '0'; ram /= 10; }
                s[idx] = '\0';
                for (int j = 0; j < idx / 2; j++) { char t = s[j]; s[j] = s[idx - j - 1]; s[idx - j - 1] = t; }
                print_at("RAM: ", 26, 7, bios_color);
                print_at(s, 31, 7, bios_color);
                print_at(" MB", 31 + idx, 7, bios_color);
            } else if (selected_item == 4) {
                print_at(" [ Exit Utility ] ", 40, 2, bios_color);
                print_at("Return to the command line interface.", 26, 4, bios_color);
            } else {
                print_at(" This section is under construction. ", 26, 4, bios_color);
            }
            print_at(" [ESC] Exit  [UP/W] [DN/S] Select  [ENTER] OK ", 2, 23, bios_color);
            needs_redraw = 0;
        }

        if (port_byte_in(0x64) & 0x01) {
            unsigned char scancode = port_byte_in(0x60);
            if (scancode & 0x80) continue;
            if (scancode == 0xE0) {
                while ((port_byte_in(0x64) & 0x01) == 0);
                scancode = port_byte_in(0x60);
                if (scancode & 0x80) continue;
            }
            if (scancode == 0x01) break;
            if (scancode == 0x48 || scancode == 0x11) {
                if (selected_item > 0) { selected_item--; needs_redraw = 1; }
            } else if (scancode == 0x50 || scancode == 0x1F) {
                if (selected_item < total_items - 1) { selected_item++; needs_redraw = 1; }
            } else if (scancode == 0x1C) {
                if (selected_item == 4) break;
            }
        }
    }
    force_reset_terminal();
}

void execute_command(char *input) {
    if (input[0] == '\0') return;

    if (compare_string(input, "EXIT") == 0) {
        kprint("Stopping CPU...\n");
        __asm__ volatile ("hlt");
    }
    else if (compare_string(input, "CLEAR") == 0) {
        force_reset_terminal();
    }
    else if (compare_string(input, "UX") == 0) {
        ux_mode();
    }
    else if (compare_string(input, "HELP") == 0) {
        kprint("System: HELP, CLEAR, EXIT, UX, INFO, TIME, LS, FORMAT, TOUCH, ECHO, SUDO, RUN, NANO, CAT, NETINIT, PING\n");
    }
    else if (compare_string(input, "INFO") == 0) {
        kprint("MIGHT OS 1.1 alpha\nSite: ssdunix.xyz\nTotal RAM: ");
        kprint_int(get_ram_size_mb());
        kprint(" MB\n");
    }
    else if (compare_string(input, "TIME") == 0) {
        uint32_t ticks = pit_get_ticks();
        uint32_t seconds = ticks / 1000;
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        seconds %= 60;
        minutes %= 60;
        hours %= 24;
        kprint("System time: ");
        if (hours < 10) kprint("0");
        kprint_int(hours);
        kprint(":");
        if (minutes < 10) kprint("0");
        kprint_int(minutes);
        kprint(":");
        if (seconds < 10) kprint("0");
        kprint_int(seconds);
        kprint("\n");
    }
    else if (compare_string(input, "LS") == 0) {
        list_files();
    }
    else if (compare_string(input, "FORMAT") == 0) {
        format_disk();
    }
    else if (compare_string(input, "NETINIT") == 0) {
        e1000_init();
    }
    else if (input[0] == 'P' && input[1] == 'I' && input[2] == 'N' && input[3] == 'G') {
        if (input[4] == ' ') {
            char* host = input + 5;
            while (*host == ' ') host++;
            ping_host(host);
        } else {
            ping_host("google.com");
        }
    }
    else if (input[0] == 'T' && input[1] == 'O' && input[2] == 'U' && input[3] == 'C' && input[4] == 'H') {
        if (input[5] == ' ') touch_file(input + 6);
        else kprint("Usage: TOUCH <filename>\n");
    }
    else if (input[0] == 'E' && input[1] == 'C' && input[2] == 'H' && input[3] == 'O') {
        if (input[4] == ' ') { kprint(input + 5); kprint("\n"); }
        else kprint("Usage: ECHO <text>\n");
    }
    else if (input[0] == 'R' && input[1] == 'U' && input[2] == 'N' && input[3] == ' ') {
        execute_external_app(input + 4);
    }
    else if (input[0] == 'S' && input[1] == 'U' && input[2] == 'D' && input[3] == 'O' && input[4] == ' ') {
        kprint("Password saved.\n");
        int i = 0;
        while (input[5 + i] != '\0' && i < 63) {
            saved_password[i] = input[5 + i];
            i++;
        }
        saved_password[i] = '\0';
    }
    else if (input[0] == 'N' && input[1] == 'A' && input[2] == 'N' && input[3] == 'O') {
        if (input[4] == ' ') nano_mode(input + 5);
        else nano_mode("");
    }
    else if (input[0] == 'C' && input[1] == 'A' && input[2] == 'T') {
        if (input[3] == ' ') {
            char file_buf[2000];
            file_buf[0] = '\0';

            char* fname = input + 4;
            while (*fname == ' ') fname++;

            read_file(fname, file_buf);

            if (file_buf[0] != '\0') {
                kprint("--- ");
                kprint(fname);
                kprint(" ---\n");
                kprint(file_buf);
                kprint("\n");
            }
        } else {
            kprint("Usage: CAT <filename>\n");
        }
    }
    else {
        kprint("Command not found. Try: HELP\n");
    }
}

void get_user_input(char* buffer) {
    int i = 0, shift_pressed = 0;
    while (1) {
        draw_top_clock();
        if (port_byte_in(0x64) & 0x01) {
            unsigned char scancode = port_byte_in(0x60);

            if (scancode == 0x01) {
                force_reset_terminal();
                kprint("> ");
                i = 0;
                continue;
            }

            if (scancode == 0xE0) {
                while ((port_byte_in(0x64) & 0x01) == 0);
                port_byte_in(0x60);
                continue;
            }
            if (scancode == 0x2A || scancode == 0x36) {
                shift_pressed = 1;
                continue;
            }
            if (scancode == (0x2A | 0x80) || scancode == (0x36 | 0x80)) {
                shift_pressed = 0;
                continue;
            }
            if (scancode & 0x80) continue;
            if (scancode == 0) continue;

            if (scancode == 0x1C) {
                buffer[i] = '\0';
                kprint("\n");
                break;
            }
            if (scancode == 0x0E) {
                if (i > 0) {
                    i--;
                    char backspace_str[2] = {'\b', '\0'};
                    kprint(backspace_str);
                }
                continue;
            }
            if (scancode < 0x80) {
                char letter;
                if (shift_pressed) letter = scancode_to_char_shift[(int)scancode];
                else letter = scancode_to_char[(int)scancode];
                if (letter != 0) {
                    buffer[i++] = letter;
                    char str[2] = {letter, '\0'};
                    kprint(str);
                }
            }
        }
    }
}

void kmain() {
    force_reset_terminal();
    idt_init();
    pit_init(1000);

    // Рисуем красивое стартовое окно
    draw_window(18, 1, 44, 6, 0x0B);
    print_at(" Welcome to MIGHT OS! ", 28, 2, 0x0F);
    print_at(" AUTHOR SITE: ssdunix.xyz ", 26, 3, 0x0E);
    print_at(" NOW IN x86-64 ", 32, 4, 0x0A);

    // Смещаем курсор вниз под графическую рамку
    kprint("\n\n\n\n\n\n\n");

    // Инициализация сети
    e1000_init();
    kprint("\nType HELP to view available commands.\n\n");

    char user_input[256];
    while (1) {
        kprint("> ");
        get_user_input(user_input);
        execute_command(user_input);
    }
}

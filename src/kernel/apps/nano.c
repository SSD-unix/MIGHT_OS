/* kernel/apps/nano.c */
#include "nano.h"
#include "../../common.h"
#include "../../drivers/lowlevel_io.h"
#include "../fs.h"
#include "../utils.h"
#include "../../drivers/window_manager.h"

// Внешние функции графики из kernel.c / window_manager.c
extern void print_at(const char* str, int x, int y, uint32_t color);
extern void fill_rect(int x, int y, int width, int height, uint32_t color);
extern void force_reset_terminal(void);

static void nano_redraw(char* buf, int len, uint32_t bg_color) {
    // Очищаем рабочую область редактора во фреймбуфере
    fill_rect(0, 20, 1024, 720, bg_color);

    int x = 10, y = 30;

    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            x = 10;
            y += 18;
            if (y > 700) break;
        } else {
            char str[2] = {buf[i], '\0'};
            print_at(str, x, y, 0xFFFFFF);
            x += 8;
            if (x >= 1000) {
                x = 10;
                y += 18;
                if (y > 700) break;
            }
        }
    }
    // Отрисовка курсора
    print_at("_", x, y, 0x00FF00);
}

void nano_mode(char* filename) {
    uint32_t bg_color = 0x181B28;
    uint32_t bar_color = 0x2E3440;

    char text_buffer[2000];
    int buf_idx = 0;
    text_buffer[0] = '\0';

    if (filename && filename[0] != '\0') {
        read_file(filename, text_buffer);
        while (text_buffer[buf_idx] != '\0' && buf_idx < 1999) {
            buf_idx++;
        }
    }

    // Заливаем экран редактора
    fill_rect(0, 0, 1024, 768, bg_color);
    fill_rect(0, 0, 1024, 20, bar_color);
    fill_rect(0, 748, 1024, 20, bar_color);

    print_at(" MIGHT nano 1.0 (GFX) ", 10, 2, 0xD8DEE9);
    if (filename && filename[0] != '\0') {
        print_at("File: ", 400, 2, 0xD8DEE9);
        print_at(filename, 450, 2, 0x88C0D0);
    } else {
        print_at("New Buffer", 400, 2, 0x88C0D0);
    }

    print_at(" [F2] Save   [ESC] Exit ", 10, 750, 0xA3BE8C);

    int shift_pressed = 0;
    nano_redraw(text_buffer, buf_idx, bg_color);

    while (1) {
        if (port_byte_in(0x64) & 0x01) {
            unsigned char scancode = port_byte_in(0x60);

            if (scancode == 0x01) break; // ESC — выходим обратно в GUI

            if (scancode == 0x3C) { // F2 — Сохранение
                if (filename && filename[0] != '\0') {
                    text_buffer[buf_idx] = '\0';
                    write_file(filename, text_buffer);
                    print_at(" [ SAVED ] ", 800, 2, 0xA3BE8C);
                } else {
                    print_at(" [ NO FILE ] ", 800, 2, 0xBF616A);
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

            if (scancode == 0x1C) { // Enter
                if (buf_idx < 1999) {
                    text_buffer[buf_idx++] = '\n';
                    nano_redraw(text_buffer, buf_idx, bg_color);
                }
                continue;
            }
            if (scancode == 0x0E) { // Backspace
                if (buf_idx > 0) {
                    buf_idx--;
                    nano_redraw(text_buffer, buf_idx, bg_color);
                }
                continue;
            }

            if (scancode < 0x80) {
                char letter = shift_pressed ? scancode_to_char_shift[(int)scancode] : scancode_to_char[(int)scancode];
                if (letter != 0 && buf_idx < 1999) {
                    text_buffer[buf_idx++] = letter;
                    nano_redraw(text_buffer, buf_idx, bg_color);
                }
            }
        }
    }
    force_reset_terminal();
}

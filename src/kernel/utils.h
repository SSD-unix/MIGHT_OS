#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include "../common.h"

// --- Прототип терминального вывода ядра ---
void kprint(const char* str);

// --- Строковые утилиты ---
int string_compare(const char* s1, const char* s2);
int string_length(const char* str);
#define compare_string string_compare

// --- Вывод и графика ---
void kprint_colored(const char *str, uint32_t color);
void draw_logo(void);
void force_reset_terminal(void);
void kprint_int(int n);
int get_ram_size_mb(void);

void draw_top_clock(void);
void fill_rect(int x, int y, int width, int height, uint32_t color);
void print_at(const char* str, int x, int y, uint32_t color);
void draw_window(int x, int y, int width, int height, uint32_t color);

// --- Раскладка клавиатуры ---
extern char scancode_to_char[];
extern char scancode_to_char_shift[];

#endif

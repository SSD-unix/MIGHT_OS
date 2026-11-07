/* kernel/utils.c */
#include "utils.h"
#include "../common.h"

// --- Массивы скан-кодов клавиатуры ---
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

// --- Данные для отрисовки ASCII/текстового арт-логотипа ---
const char *char_map[] = {
    "__##", "#", "##____", "#", "###", "#\n",
    "__#", "#", "#", "#", "#____", "#", "###", "#\n",
    "___##", "#______", "###\n",
    "___#_#______#_#\n"
};

uint32_t color_map[] = {
    0x0000BB, 0x0000BB, 0x006666, 0x0000BB, 0x00FFFF, 0x0000BB, 0x006666, 0x00FFFF, 0x006666,
    0x00FFFF, 0x000000, 0x00FFFF, 0x000000, 0x00FFFF, 0x00FFFF, 0x000000, 0x00FFFF, 0x000000, 0x00FFFF,
    0x00FFFF, 0x000000, 0x00FFFF, 0x000000, 0x00FFFF, 0x00FFFF, 0x000000, 0x00FFFF, 0x000000, 0x00FFFF,
    0x00FFFF, 0x00FFFF, 0x00FFFF, 0x00FFFF, 0x007777, 0x0000BB, 0x007777, 0x00EEEE,
    0x007777, 0x0000BB, 0x007777, 0x00FFFF, 0x00EEEE, 0x00FFFF,
    0x007777, 0x0000BB, 0x007777, 0x00FFFF, 0x00EEEE, 0x00FFFF,
    0x00FFFF, 0x007777, 0x0000BB, 0x007777, 0x00FFFF, 0x00FFFF, 0x00EEEE, 0x00FFFF,
    0x009999, 0x007777, 0x009999, 0x009999
};

// Цветная печать строки через kprint()
void kprint_colored(const char *str, uint32_t color) {
    (void)color; // Цвет передаётся напрямую через параметры терминала при необходимости
    while (*str) {
        if (*str == '_') {
            kprint(" ");
        } else {
            char ch_str[2] = {*str, '\0'};
            kprint(ch_str);
        }
        str++;
    }
}

// Отрисовка арт-логотипа
void draw_logo(void) {
    for (int i = 0; i < 18; i++) {
        kprint_colored(char_map[i], color_map[i]);
    }
}

// Строковые функции
int string_compare(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') return 0;
        i++;
    }
    return (unsigned char)s1[i] - (unsigned char)s2[i];
}

int string_length(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

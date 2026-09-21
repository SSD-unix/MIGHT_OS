#ifndef SCREEN_H
#define SCREEN_H

#include <stddef.h>

#define VIDEO_ADDRESS 0xb8000
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0f
#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

void kprint(const char *str);
void putchar(char character, unsigned char attribute_byte);
void clear_screen();
void write(char character, unsigned char attribute_byte, unsigned short offset);
void scroll_line();
unsigned short get_cursor();
void set_cursor(unsigned short pos);

#endif

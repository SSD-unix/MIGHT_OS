#include "screen.h"
#include "lowlevel_io.h"

void kprint(const char *str) {
    while (*str) {
        putchar(*str, WHITE_ON_BLACK);
        str++;
    }
}

void putchar(char character, unsigned char attribute_byte) {
    unsigned short offset = get_cursor();
    if (character == '\n') {
        if ((offset / 2 / MAX_COLS) == (MAX_ROWS - 1))
            scroll_line();
        else
            set_cursor((offset - offset % (MAX_COLS * 2)) + MAX_COLS * 2);
    } else if (character == '\b') {
        if (offset >= 2) {
            offset -= 2;
            write(' ', attribute_byte, offset);
            set_cursor(offset);
        }
    } else {
        if (offset >= (MAX_COLS * MAX_ROWS * 2)) {
            scroll_line();
            offset = get_cursor();
        }
        write(character, attribute_byte, offset);
        set_cursor(offset + 2);
    }
}

void scroll_line() {
    unsigned char i = 1;
    unsigned short last_line;
    while (i < MAX_ROWS) {
        for (int j = 0; j < MAX_COLS * 2; j++)
            *(unsigned char*)(VIDEO_ADDRESS + (MAX_COLS * (i - 1) * 2) + j) =
                *(unsigned char*)(VIDEO_ADDRESS + (MAX_COLS * i * 2) + j);
        i++;
    }
    last_line = (MAX_COLS * MAX_ROWS * 2) - MAX_COLS * 2;
    for (int j = 0; j < MAX_COLS * 2; j++)
        *(unsigned char*)(VIDEO_ADDRESS + last_line + j) = 0;
    set_cursor(last_line);
}

void clear_screen() {
    unsigned short offset = 0;
    while (offset < (MAX_ROWS * MAX_COLS * 2)) {
        write(' ', WHITE_ON_BLACK, offset);
        offset += 2;
    }
    set_cursor(0);
}

void write(char character, unsigned char attribute_byte, unsigned short offset) {
    unsigned char *vga = (unsigned char*)VIDEO_ADDRESS;
    vga[offset] = character;
    vga[offset + 1] = attribute_byte;
}

unsigned short get_cursor() {
    port_byte_out(REG_SCREEN_CTRL, 14);
    unsigned char high_byte = port_byte_in(REG_SCREEN_DATA);
    port_byte_out(REG_SCREEN_CTRL, 15);
    unsigned char low_byte = port_byte_in(REG_SCREEN_DATA);
    return ((high_byte << 8) + low_byte) * 2;
}

void set_cursor(unsigned short pos) {
    pos /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(pos >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(pos & 0xff));
}

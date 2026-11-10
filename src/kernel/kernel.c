/* kernel/kernel.c - MIGHT OS Kernel Core (UEFI GOP & Multiboot2 Direct LFB) */
#include "../common.h"
#include "../drivers/lowlevel_io.h"
#include "../drivers/pit.h"
#include "../drivers/idt.h"
#include "../drivers/e1000.h"
#include "fs.h"
#include "utils.h"
#include "../drivers/window_manager.h"
#include "task.h"
#include "../drivers/wget.h"

#include <emmintrin.h> // SSE2 intrinsics для копирования кадров

#define COM1_PORT 0x3F8

#define SYS_WRITE      1
#define SYS_READ_CHAR  2
#define SYS_CLEAR      3
#define SYS_TIME       4
#define SYS_READ_FILE  5
#define SYS_WRITE_FILE 6
#define SYS_EXIT       60

/* GUI System Calls */
#define SYS_CREATE_WINDOW 10
#define SYS_DRAW_RECT     11
#define SYS_DRAW_TEXT     12
#define SYS_DRAW_BUTTON   13
#define SYS_POLL_EVENT    14
#define SYS_REDRAW_WIN    15

#define FB_MAX_WIDTH  1920
#define FB_MAX_HEIGHT 1080
#define FONT_W        8
#define FONT_H        16
/* network */
#define SYS_HTTP_GET      20

char saved_password[64] = {0};

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
} __attribute__((packed));

struct multiboot_tag_framebuffer sys_fb;

/* Двойной буфер в RAM (выровнен по границе 4KB для эффективности страницы) */
static uint32_t backbuffer[FB_MAX_WIDTH * FB_MAX_HEIGHT] __attribute__((aligned(4096)));

const uint8_t font8x16_basic[128][16] = {
    ['A'] = {0x00,0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x42,0x42,0x00},
    ['B'] = {0x00,0x7C,0x42,0x42,0x7C,0x42,0x42,0x42,0x7C,0x00},
    ['C'] = {0x00,0x3C,0x42,0x40,0x40,0x40,0x40,0x42,0x3C,0x00},
    ['D'] = {0x00,0x78,0x44,0x42,0x42,0x42,0x42,0x44,0x78,0x00},
    ['E'] = {0x00,0x7C,0x40,0x40,0x78,0x40,0x40,0x40,0x7C,0x00},
    ['F'] = {0x00,0x7C,0x40,0x40,0x78,0x40,0x40,0x40,0x40,0x00},
    ['G'] = {0x00,0x3C,0x42,0x40,0x4E,0x42,0x42,0x42,0x3C,0x00},
    ['H'] = {0x00,0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x42,0x00},
    ['I'] = {0x00,0x3C,0x10,0x10,0x10,0x10,0x10,0x10,0x3C,0x00},
    ['J'] = {0x00,0x1E,0x08,0x08,0x08,0x08,0x08,0x48,0x30,0x00},
    ['K'] = {0x00,0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x42,0x00},
    ['L'] = {0x00,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x7C,0x00},
    ['M'] = {0x00,0x42,0x66,0x5A,0x42,0x42,0x42,0x42,0x42,0x00},
    ['N'] = {0x00,0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x42,0x00},
    ['O'] = {0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},
    ['P'] = {0x00,0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x40,0x00},
    ['Q'] = {0x00,0x3C,0x42,0x42,0x42,0x42,0x4A,0x44,0x3A,0x00},
    ['R'] = {0x00,0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x42,0x00},
    ['S'] = {0x00,0x3C,0x42,0x40,0x3C,0x02,0x02,0x42,0x3C,0x00},
    ['T'] = {0x00,0x7E,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x00},
    ['U'] = {0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},
    ['V'] = {0x00,0x42,0x42,0x42,0x42,0x24,0x24,0x18,0x18,0x00},
    ['W'] = {0x00,0x42,0x42,0x42,0x42,0x42,0x5A,0x66,0x42,0x00},
    ['X'] = {0x00,0x42,0x24,0x18,0x18,0x18,0x24,0x42,0x42,0x00},
    ['Y'] = {0x00,0x42,0x42,0x24,0x18,0x10,0x10,0x10,0x10,0x00},
    ['Z'] = {0x00,0x7E,0x02,0x04,0x08,0x10,0x20,0x40,0x7E,0x00},
    ['f'] = {0x00,0x0E,0x10,0x7C,0x10,0x10,0x10,0x10,0x38,0x00},
    ['j'] = {0x00,0x02,0x00,0x06,0x02,0x02,0x02,0x42,0x3C,0x00},
    ['k'] = {0x00,0x40,0x40,0x42,0x44,0x78,0x44,0x42,0x42,0x00},
    ['v'] = {0x00,0x00,0x00,0x42,0x42,0x42,0x24,0x24,0x18,0x00},
    ['w'] = {0x00,0x00,0x00,0x42,0x42,0x42,0x5A,0x66,0x24,0x00},
    ['z'] = {0x00,0x00,0x00,0x7E,0x04,0x08,0x10,0x20,0x7E,0x00},
    ['a'] = {0x00,0x00,0x00,0x3C,0x02,0x3E,0x42,0x46,0x3A,0x00},
    ['b'] = {0x00,0x40,0x40,0x5C,0x62,0x42,0x42,0x62,0x5C,0x00},
    ['c'] = {0x00,0x00,0x00,0x3C,0x42,0x40,0x40,0x42,0x3C,0x00},
    ['d'] = {0x00,0x02,0x02,0x3A,0x46,0x42,0x42,0x46,0x3A,0x00},
    ['e'] = {0x00,0x00,0x00,0x3C,0x42,0x7E,0x40,0x42,0x3C,0x00},
    ['g'] = {0x00,0x00,0x3A,0x46,0x42,0x46,0x3A,0x02,0x3C,0x00},
    ['h'] = {0x00,0x40,0x40,0x5C,0x62,0x42,0x42,0x42,0x42,0x00},
    ['i'] = {0x00,0x10,0x00,0x30,0x10,0x10,0x10,0x10,0x38,0x00},
    ['l'] = {0x00,0x30,0x10,0x10,0x10,0x10,0x10,0x10,0x38,0x00},
    ['m'] = {0x00,0x00,0x00,0x58,0x66,0x52,0x52,0x52,0x52,0x00},
    ['n'] = {0x00,0x00,0x00,0x5C,0x62,0x42,0x42,0x42,0x42,0x00},
    ['o'] = {0x00,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00},
    ['p'] = {0x00,0x00,0x00,0x5C,0x62,0x42,0x62,0x5C,0x40,0x40},
    ['r'] = {0x00,0x00,0x00,0x54,0x68,0x40,0x40,0x40,0x40,0x00},
    ['s'] = {0x00,0x00,0x00,0x3E,0x40,0x3C,0x02,0x42,0x3C,0x00},
    ['t'] = {0x00,0x10,0x10,0x7E,0x10,0x10,0x10,0x12,0x0C,0x00},
    ['u'] = {0x00,0x00,0x00,0x42,0x42,0x42,0x42,0x46,0x3A,0x00},
    ['x'] = {0x00,0x00,0x00,0x42,0x24,0x18,0x18,0x24,0x42,0x00},
    ['y'] = {0x00,0x00,0x00,0x42,0x42,0x46,0x3A,0x02,0x3C,0x00},
    ['0'] = {0x00,0x3C,0x46,0x4A,0x52,0x62,0x3C,0x00,0x00,0x00},
    ['1'] = {0x00,0x18,0x28,0x08,0x08,0x08,0x3E,0x00,0x00,0x00},
    ['2'] = {0x00,0x3C,0x42,0x02,0x1C,0x20,0x7E,0x00,0x00,0x00},
    ['3'] = {0x00,0x3C,0x42,0x0C,0x02,0x42,0x3C,0x00,0x00,0x00},
    ['4'] = {0x00,0x08,0x18,0x28,0x48,0x7E,0x08,0x00,0x00,0x00},
    ['5'] = {0x00,0x7E,0x40,0x7C,0x02,0x42,0x3C,0x00,0x00,0x00},
    ['6'] = {0x00,0x3C,0x40,0x7C,0x42,0x42,0x3C,0x00,0x00,0x00},
    ['7'] = {0x00,0x7E,0x02,0x04,0x08,0x10,0x10,0x00,0x00,0x00},
    ['8'] = {0x00,0x3C,0x42,0x3C,0x42,0x42,0x3C,0x00,0x00,0x00},
    ['9'] = {0x00,0x3C,0x42,0x3E,0x02,0x02,0x3C,0x00,0x00,0x00},
    [':'] = {0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00},
    ['.'] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00},
    ['-'] = {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00},
    ['_'] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00},
    ['>'] = {0x00,0x40,0x20,0x10,0x08,0x10,0x20,0x40,0x00,0x00},
    ['<'] = {0x00,0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x00,0x00},
    ['='] = {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00,0x00,0x00},
    ['/'] = {0x00,0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00,0x00},
    ['!'] = {0x00,0x18,0x18,0x18,0x18,0x00,0x18,0x00,0x00,0x00},
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};

/* --- Последовательный порт (COM1) для вывода логов --- */
void serial_init(void) {
    port_byte_out(COM1_PORT + 1, 0x00);
    port_byte_out(COM1_PORT + 3, 0x80);
    port_byte_out(COM1_PORT + 0, 0x03);
    port_byte_out(COM1_PORT + 1, 0x00);
    port_byte_out(COM1_PORT + 3, 0x03);
    port_byte_out(COM1_PORT + 2, 0xC7);
    port_byte_out(COM1_PORT + 4, 0x0B);
}

static inline int is_transmit_empty(void) {
    return port_byte_in(COM1_PORT + 5) & 0x20;
}

void write_serial(char c) {
    while (is_transmit_empty() == 0);
    port_byte_out(COM1_PORT, c);
}

void serial_print(const char* str) {
    if (!str) return;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') write_serial('\r');
        write_serial(str[i]);
    }
}

/* --- Включение SSE в контрольном регистре CR4 --- */
void enable_sse(void) {
    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9) | (1 << 10); // Bit 9: OSFXSR, Bit 10: OSXMMEXCPT
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
}

void clear_full_backbuffer(uint32_t color) {
    uint32_t w = sys_fb.framebuffer_width ? sys_fb.framebuffer_width : 1024;
    uint32_t h = sys_fb.framebuffer_height ? sys_fb.framebuffer_height : 768;
    if (w > FB_MAX_WIDTH) w = FB_MAX_WIDTH;
    if (h > FB_MAX_HEIGHT) h = FB_MAX_HEIGHT;

    for (uint32_t y = 0; y < h; y++) {
        uint32_t *row = &backbuffer[y * FB_MAX_WIDTH];
        for (uint32_t x = 0; x < w; x++) {
            row[x] = color;
        }
    }
}

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    uint32_t w = sys_fb.framebuffer_width ? sys_fb.framebuffer_width : 1024;
    uint32_t h = sys_fb.framebuffer_height ? sys_fb.framebuffer_height : 768;

    if (x >= w || y >= h || x >= FB_MAX_WIDTH || y >= FB_MAX_HEIGHT) return;

    backbuffer[y * FB_MAX_WIDTH + x] = color;
}

/* Оптимизированный перенос буфера из RAM в физическую VRAM (GOP Framebuffer) */
void flip_framebuffer(void) {
    if (!sys_fb.framebuffer_addr || !sys_fb.framebuffer_pitch) return;

    uint8_t *vram = (uint8_t*)(uintptr_t)sys_fb.framebuffer_addr;
    uint32_t w = sys_fb.framebuffer_width ? sys_fb.framebuffer_width : 1024;
    uint32_t h = sys_fb.framebuffer_height ? sys_fb.framebuffer_height : 768;
    uint32_t pitch = sys_fb.framebuffer_pitch;

    if (w > FB_MAX_WIDTH) w = FB_MAX_WIDTH;
    if (h > FB_MAX_HEIGHT) h = FB_MAX_HEIGHT;

    uint32_t row_bytes = w * 4;

    for (uint32_t y = 0; y < h; y++) {
        uint8_t *dst_row = vram + (y * pitch);
        uint8_t *src_row = (uint8_t*)&backbuffer[y * FB_MAX_WIDTH];

        size_t i = 0;
        /* SSE2 Векторная выгрузка без требования к 16-байтовому выравниванию VRAM */
        for (; i + 16 <= row_bytes; i += 16) {
            __m128i chunk = _mm_loadu_si128((__m128i*)(src_row + i));
            _mm_storeu_si128((__m128i*)(dst_row + i), chunk);
        }
        /* Оставшиеся пиксели строки */
        for (; i < row_bytes; i++) {
            dst_row[i] = src_row[i];
        }
    }
}

void draw_char_gfx(char c, uint32_t start_x, uint32_t start_y, uint32_t color) {
    for (int r = 0; r < 16; r++) {
        uint8_t row = font8x16_basic[(unsigned char)c][r];
        for (int col = 0; col < 8; col++) {
            if (row & (1 << (7 - col))) {
                draw_pixel(start_x + col, start_y + r, color);
            }
        }
    }
}

void print_at(const char* str, int x, int y, uint32_t color) {
    if (!str) return;
    int i = 0;
    while (str[i] != '\0') {
        draw_char_gfx(str[i], x + (i * FONT_W), y, color);
        i++;
    }
}

void fill_rect(int x, int y, int width, int height, uint32_t color) {
    for (int r = y; r < y + height; r++) {
        for (int c = x; c < x + width; c++) {
            draw_pixel(c, r, color);
        }
    }
}

void draw_window(int x, int y, int width, int height, uint32_t color) {
    for (int i = 0; i < width; i++) {
        draw_pixel(x + i, y, color);
        draw_pixel(x + i, y + height - 1, color);
    }
    for (int i = 0; i < height; i++) {
        draw_pixel(x, y + i, color);
        draw_pixel(x + width - 1, y + i, color);
    }
}

void kprint(const char* str) {
    wm_terminal_print(str);
    serial_print(str);
}

void force_reset_terminal(void) {
    wm_terminal_clear();
}

void kprint_int(int n) {
    if (n == 0) { kprint("0"); return; }
    char s[12]; int i = 0;
    while (n > 0) { s[i++] = (n % 10) + '0'; n /= 10; }
    s[i] = '\0';
    for (int j = 0; j < i / 2; j++) { char temp = s[j]; s[j] = s[i - j - 1]; s[i - j - 1] = temp; }
    kprint(s);
}

int get_ram_size_mb(void) {
    unsigned char low, high;
    port_byte_out(0x70, 0x30); low = port_byte_in(0x71);
    port_byte_out(0x70, 0x31); high = port_byte_in(0x71);
    return (((high << 8) | low) + 1024) / 1024;
}

uint64_t syscall_dispatcher(uint64_t sys_num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    if (sys_num == SYS_WRITE) {
        char* buf = (char*)a2;
        int count = (int)a3;
        for (int i = 0; i < count; i++) {
            char str[2] = {buf[i], '\0'};
            kprint(str);
        }
        return 0;
    }
    else if (sys_num == SYS_READ_CHAR) {
        if (port_byte_in(0x64) & 0x01) {
            uint8_t scancode = port_byte_in(0x60);
            if (!(scancode & 0x80)) {
                char c = scancode_to_char[(int)scancode];
                return (uint64_t)(unsigned char)c;
            }
        }
        schedule();
        return 0;
    }
    else if (sys_num == SYS_EXIT) {
        kprint("\n[App Exited]\n");
        task_exit();
        return 0;
    }

    /* === GUI SYSCALLS === */
    else if (sys_num == SYS_CREATE_WINDOW) {
        const char* title = (const char*)a1;
        int w = (int)a2, h = (int)a3, x = (int)a4, y = (int)a5;
        int win_id = wm_create_window(x, y, w, h, title, 0x1E222A);

        task_t* cur = task_get_current();
        if (cur) cur->win_id = win_id;
        return (uint64_t)win_id;
    }
    else if (sys_num == SYS_DRAW_RECT) {
        wm_draw_window_rect((int)a1, (int)a2, (int)a3, (int)a4, (int)(a5 & 0xFFFF), (uint32_t)(a5 >> 32));
        return 0;
    }
    else if (sys_num == SYS_DRAW_TEXT) {
        wm_draw_window_text((int)a1, (int)a2, (int)a3, (const char*)a4, (uint32_t)a5);
        return 0;
    }
    else if (sys_num == SYS_DRAW_BUTTON) {
        int win_id = (int)a1;
        int rel_x = (int)a2;
        int rel_y = (int)a3;
        const char* text = (const char*)a4;
        int w = (int)(a5 & 0xFFFF);
        int h = (int)((a5 >> 16) & 0xFFFF);
        uint32_t bg_color = (uint32_t)(a5 >> 32);

        wm_draw_window_button(win_id, rel_x, rel_y, w, h, text, bg_color);
        return 0;
    }
    else if (sys_num == SYS_POLL_EVENT) {
        int win_id = (int)a1;
        struct gui_event* ev = (struct gui_event*)a2;
        int res = wm_poll_event(win_id, ev);

        if (res == 0) {
            schedule();
            __asm__ volatile ("hlt");
        }
        return (uint64_t)res;
    }
    else if (sys_num == SYS_REDRAW_WIN) {
        wm_draw_scene();
        flip_framebuffer();
        return 0;
    }

    /* === NETWORKING SYSCALL === */
    else if (sys_num == SYS_HTTP_GET) {
        const char* ip = (const char*)a1;
        const char* path = (const char*)a2;

        if (!path || path[0] == '\0') path = "/";

        wget_download(ip, path);
        return 0;
    }

    return 0;
}

#define MAX_APPS 4
#define APP_BUFFER_SIZE (8 * 1024 * 1024) // 8 MB под бинарник, BSS и Heap браузера
static uint8_t app_buffers[MAX_APPS][APP_BUFFER_SIZE] __attribute__((aligned(4096)));
static int app_buffer_idx = 0;

void execute_external_app(char* filename) {
    if (!filename || filename[0] == '\0') {
        kprint("Error: No filename specified.\n");
        return;
    }

    int buf_slot = app_buffer_idx;
    app_buffer_idx = (app_buffer_idx + 1) % MAX_APPS;

    /* Обнуляем весь буфер перед загрузкой, чтобы очистить BSS и кучу бинарника */
    for (size_t i = 0; i < APP_BUFFER_SIZE; i++) {
        app_buffers[buf_slot][i] = 0;
    }

    read_file(filename, (char*)app_buffers[buf_slot]);

    if (app_buffers[buf_slot][0] == 0) {
        kprint("Error: File '");
        kprint(filename);
        kprint("' not found or buffer empty.\n");
        return;
    }

    kprint("[Task Manager] Spawning process for ");
    kprint(filename);
    kprint("...\n");

    typedef void (*app_entry_t)(void*);
    app_entry_t entry = (app_entry_t)(uintptr_t)app_buffers[buf_slot];

    int task_id = task_create(entry, (void*)syscall_dispatcher, filename);
    if (task_id < 0) {
        kprint("Error: Task limit reached (MAX_TASKS).\n");
        return;
    }

    kprint("[Task Manager] Started Process PID: ");
    kprint_int(task_id);
    kprint("\n");
}

void execute_command(char *input) {
    if (!input || input[0] == '\0') return;

    if (compare_string(input, "EXIT") == 0) {
        kprint("Stopping CPU...\n");
        __asm__ volatile ("hlt");
    }
    else if (compare_string(input, "CLEAR") == 0) {
        force_reset_terminal();
    }
    else if (compare_string(input, "HELP") == 0) {
        kprint("System: HELP, CLEAR, EXIT, INFO, TIME, LS, FORMAT, TOUCH, MKDIR, RM, ECHO, SUDO, RUN, CAT, NETINIT, PING, WGET\n");
    }
    else if (compare_string(input, "INFO") == 0) {
        kprint("MIGHT OS 1.1 Desktop\nRAM: ");
        kprint_int(get_ram_size_mb());
        kprint(" MB\n");
    }
    else if (compare_string(input, "TIME") == 0) {
        uint32_t ticks = pit_get_ticks();
        uint32_t seconds = ticks / 30;
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        seconds %= 60; minutes %= 60; hours %= 24;
        kprint("Time: ");
        if (hours < 10) { kprint("0"); }
        kprint_int(hours); kprint(":");
        if (minutes < 10) { kprint("0"); }
        kprint_int(minutes); kprint(":");
        if (seconds < 10) { kprint("0"); }
        kprint_int(seconds); kprint("\n");
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
    else if (input[0] == 'W' && input[1] == 'G' && input[2] == 'E' && input[3] == 'T') {
        if (input[4] == ' ') {
            char* args = input + 5;
            while (*args == ' ') args++;

            // Разделяем IP и PATH
            char ip_buf[32];
            char path_buf[64];
            ip_buf[0] = '\0';
            path_buf[0] = '\0';

            int i = 0;
            while (args[i] != ' ' && args[i] != '\0' && i < 31) {
                ip_buf[i] = args[i];
                i++;
            }
            ip_buf[i] = '\0';

            if (args[i] == ' ') {
                char* p = args + i + 1;
                while (*p == ' ') p++;
                int j = 0;
                while (*p != '\0' && j < 63) {
                    path_buf[j++] = *p++;
                }
                path_buf[j] = '\0';
            } else {
                path_buf[0] = '/';
                path_buf[1] = '\0';
            }

            wget_download(ip_buf, path_buf);
        } else {
            kprint("Usage: WGET <IP> <path>\nExample: WGET 10.0.2.2 /\n");
        }
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
    else if (input[0] == 'M' && input[1] == 'K' && input[2] == 'D' && input[3] == 'I' && input[4] == 'R') {
        if (input[5] == ' ') make_directory(input + 6);
        else kprint("Usage: MKDIR <dirname>\n");
    }
    else if (input[0] == 'R' && input[1] == 'M') {
        if (input[2] == ' ') remove_file(input + 3);
        else kprint("Usage: RM <filename>\n");
    }
    else if (input[0] == 'C' && input[1] == 'A' && input[2] == 'T') {
        if (input[3] == ' ') {
            char file_buf[2000];
            file_buf[0] = '\0';
            char* fname = input + 4;
            while (*fname == ' ') fname++;
            read_file(fname, file_buf);
            if (file_buf[0] != '\0') {
                kprint("--- "); kprint(fname); kprint("\n");
                kprint(file_buf); kprint("\n");
            }
        } else {
            kprint("Usage: CAT <filename>\n");
        }
    }
    else {
        kprint("Command not found. Try: HELP\n");
    }
}

static char cmd_buffer[128];
static int cmd_idx = 0;
static int shift_state = 0;
static int alt_state = 0;

void process_keyboard_events(void) {
    if ((port_byte_in(0x64) & 0x01) == 0) return;

    uint8_t scancode = port_byte_in(0x60);

    if (scancode == 0x2A || scancode == 0x36) { shift_state = 1; return; }
    if (scancode == (0x2A | 0x80) || scancode == (0x36 | 0x80)) { shift_state = 0; return; }
    if (scancode == 0x38) { alt_state = 1; return; }
    if (scancode == (0x38 | 0x80)) { alt_state = 0; return; }

    if (scancode & 0x80) return;

    if (alt_state && scancode == 0x0F) {
        wm_select_next_window();
        return;
    }

    if (alt_state) {
        if (scancode == 0x4B) { wm_move_active_window(-10, 0); return; }
        if (scancode == 0x4D) { wm_move_active_window(10, 0);  return; }
        if (scancode == 0x48) { wm_move_active_window(0, -10); return; }
        if (scancode == 0x50) { wm_move_active_window(0, 10);  return; }
    }

    if (scancode == 0x4B) { wm_move_cursor(-10, 0); return; }
    if (scancode == 0x4D) { wm_move_cursor(10, 0);  return; }
    if (scancode == 0x48) { wm_move_cursor(0, -10); return; }
    if (scancode == 0x50) { wm_move_cursor(0, 10);  return; }

    if (scancode == 0x1C) {
        cmd_buffer[cmd_idx] = '\0';
        kprint("\n");
        execute_command(cmd_buffer);
        cmd_idx = 0;
        kprint("> ");
        return;
    }

    if (scancode == 0x0E) {
        if (cmd_idx > 0) {
            cmd_idx--;
            char bsp[2] = {'\b', '\0'};
            kprint(bsp);
        }
        return;
    }

    char ch = shift_state ? scancode_to_char_shift[(int)scancode] : scancode_to_char[(int)scancode];
    if (ch != 0 && cmd_idx < 120) {
        cmd_buffer[cmd_idx++] = ch;
        char str[2] = {ch, '\0'};
        kprint(str);
    }
}

void kmain(uint64_t multiboot_addr) {
    enable_sse();   // Первым делом включаем SSE (OSFXSR) в CR4
    serial_init();  // Инициализируем COM1 порт для логов QEMU (-serial stdio)
    idt_init();

    if (multiboot_addr != 0) {
        struct multiboot_tag *tag;
        for (tag = (struct multiboot_tag *)(multiboot_addr + 8);
             tag->type != 0;
             tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7)))
        {
            if (tag->type == 8) {
                struct multiboot_tag_framebuffer *fb = (struct multiboot_tag_framebuffer *)tag;
                sys_fb = *fb;
                break;
            }
        }
    }

    if (sys_fb.framebuffer_width == 0) sys_fb.framebuffer_width = 1024;
    if (sys_fb.framebuffer_height == 0) sys_fb.framebuffer_height = 768;
    if (sys_fb.framebuffer_pitch == 0) sys_fb.framebuffer_pitch = sys_fb.framebuffer_width * 4;

    wm_init();
    task_init();

    wm_create_window(80, 80, 460, 280, "MIGHT OS Terminal", 0x181B28);
    wm_create_window(560, 120, 320, 200, "System Info", 0x1E222A);

    kprint("MIGHT OS GUI Initialized.\nFB Mode: ");
    kprint_int(sys_fb.framebuffer_width);
    kprint("x");
    kprint_int(sys_fb.framebuffer_height);
    kprint(" Pitch: ");
    kprint_int(sys_fb.framebuffer_pitch);
    kprint("\nType 'HELP' for commands.\n\n> ");

    pit_init(30);
    e1000_init();

    __asm__ volatile ("sti");

    uint32_t last_tick = 0;

    while (1) {
        process_keyboard_events();

        uint32_t current_tick = pit_get_ticks();
        if (current_tick != last_tick) {
            last_tick = current_tick;

            clear_full_backbuffer(0x0F111A);
            wm_draw_scene();
            flip_framebuffer();
        }

        schedule();
        __asm__ volatile ("hlt");
    }
}

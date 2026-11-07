/* drivers/window_manager.c - MIGHT OS Window Manager (UEFI GOP Compatible) */
#include "window_manager.h"
#include "../common.h"
#include "../drivers/lowlevel_io.h"
#include <stdint.h>
#include "font_hy.h"
#include "bg_image.h"

#define MAX_WINDOWS 8
#define TERM_ROWS   14
#define TERM_COLS   52
#define FONT_W      8
#define KBD_BUF_SIZE 32
#define BG_WIDTH    800
#define BG_HEIGHT   600

#define MAX_WINDOW_CMDS 64

typedef enum {
    CMD_NONE = 0,
    CMD_RECT,
    CMD_TEXT,
    CMD_BUTTON
} draw_cmd_type_t;

typedef struct {
    draw_cmd_type_t type;
    int x, y, w, h;
    uint32_t color;
    char text[64];
} draw_cmd_t;

typedef struct {
    int x, y, width, height;
    char title[32];
    uint32_t bg_color;
    int active;
    int visible;
    int custom_gui;
    draw_cmd_t cmds[MAX_WINDOW_CMDS];
    int cmd_count;
} window_t;

static window_t windows[MAX_WINDOWS];
static int window_count = 0;
static int active_window_idx = 0;

static char kbd_buffer[KBD_BUF_SIZE];
static int kbd_head = 0;
static int kbd_tail = 0;
static int alt_state = 0;

void wm_push_key(char c) {
    int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        kbd_head = next;
    }
}

static char wm_pop_key(void) {
    if (kbd_head == kbd_tail) return 0;
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return c;
}

extern void draw_pixel(uint32_t x, uint32_t y, uint32_t color);
extern void fill_rect(int x, int y, int width, int height, uint32_t color);
extern void print_at(const char* str, int x, int y, uint32_t color);
extern void draw_window(int x, int y, int width, int height, uint32_t color);
extern char scancode_to_char[];

void draw_char_hy(uint16_t code, int x, int y, uint32_t color) {
    if (code < 0x0531 || code > 0x058F) return;
    int idx = code - 0x0531;

    for (int r = 0; r < 16; r++) {
        uint8_t row = font_hy_16[idx][r];
        for (int c = 0; c < 8; c++) {
            if (row & (1 << (7 - c))) {
                draw_pixel((uint32_t)(x + c), (uint32_t)(y + r), color);
            }
        }
    }
}

void print_hy_at(const char* str, int x, int y, uint32_t color) {
    int cur_x = x;
    uint8_t* p = (uint8_t*)str;

    while (*p) {
        if (*p < 0x80) {
            char single_char[2] = {(char)*p, '\0'};
            print_at(single_char, cur_x, y, color);
            p++;
        } else if ((*p & 0xE0) == 0xC0) {
            uint16_t unicode_val = ((*p & 0x1F) << 6) | (*(p + 1) & 0x3F);
            draw_char_hy(unicode_val, cur_x, y, color);
            p += 2;
        } else {
            p++;
        }
        cur_x += 8;
    }
}

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

extern struct multiboot_tag_framebuffer sys_fb;

static char term_buffer[TERM_ROWS][TERM_COLS + 1];
static int term_row = 0;
static int term_col = 0;

static int mouse_x = 400;
static int mouse_y = 300;

void wm_draw_background(void) {
    uint32_t sw = sys_fb.framebuffer_width ? sys_fb.framebuffer_width : 1024;
    uint32_t sh = sys_fb.framebuffer_height ? sys_fb.framebuffer_height : 768;

    uint32_t x_ratio = ((BG_WIDTH << 16) / sw) + 1;
    uint32_t y_ratio = ((BG_HEIGHT << 16) / sh) + 1;

    for (uint32_t y = 0; y < sh; y++) {
        uint32_t img_y = (y * y_ratio) >> 16;
        if (img_y >= BG_HEIGHT) img_y = BG_HEIGHT - 1;
        uint32_t bg_row_offset = img_y * BG_WIDTH * 3;

        for (uint32_t x = 0; x < sw; x++) {
            uint32_t img_x = (x * x_ratio) >> 16;
            if (img_x >= BG_WIDTH) img_x = BG_WIDTH - 1;

            uint32_t index = bg_row_offset + (img_x * 3);

            uint8_t r = bg_raw[index];
            uint8_t g = bg_raw[index + 1];
            uint8_t b = bg_raw[index + 2];

            uint32_t color = (r << 16) | (g << 8) | b;
            draw_pixel(x, y, color);
        }
    }
}

void wm_terminal_clear(void) {
    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            term_buffer[r][c] = ' ';
        }
        term_buffer[r][TERM_COLS] = '\0';
    }
    term_row = 0;
    term_col = 0;
}

void wm_init(void) {
    window_count = 0;
    active_window_idx = 0;
    mouse_x = 400;
    mouse_y = 300;
    kbd_head = 0;
    kbd_tail = 0;
    alt_state = 0;
    wm_terminal_clear();
}

int wm_create_window(int x, int y, int w, int h, const char* title, uint32_t color) {
    if (window_count >= MAX_WINDOWS) return -1;
    int idx = window_count++;
    windows[idx].x = x;
    windows[idx].y = y;
    windows[idx].width = w;
    windows[idx].height = h;
    windows[idx].bg_color = color;
    windows[idx].active = 1;
    windows[idx].visible = 1;
    windows[idx].custom_gui = (idx >= 2) ? 1 : 0;
    windows[idx].cmd_count = 0;

    for (int i = 0; i < window_count - 1; i++) {
        windows[i].active = 0;
    }
    active_window_idx = idx;

    int i = 0;
    if (title) {
        while (title[i] != '\0' && i < 31) {
            windows[idx].title[i] = title[i];
            i++;
        }
    }
    windows[idx].title[i] = '\0';
    return idx;
}

void wm_destroy_window(int win_id) {
    if (win_id < 0 || win_id >= window_count) return;
    windows[win_id].visible = 0;
    if (win_id == active_window_idx && window_count > 1) {
        wm_select_next_window();
    }
}

void wm_destroy_active_window(void) {
    if (window_count <= 1) return;
    wm_destroy_window(active_window_idx);
}

void wm_terminal_print(const char* str) {
    if (!str) return;
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (c == '\n') {
            term_col = 0;
            term_row++;
            if (term_row >= TERM_ROWS) {
                for (int r = 0; r < TERM_ROWS - 1; r++) {
                    for (int col = 0; col < TERM_COLS; col++) {
                        term_buffer[r][col] = term_buffer[r + 1][col];
                    }
                    term_buffer[r][TERM_COLS] = '\0';
                }
                for (int col = 0; col < TERM_COLS; col++) {
                    term_buffer[TERM_ROWS - 1][col] = ' ';
                }
                term_buffer[TERM_ROWS - 1][TERM_COLS] = '\0';
                term_row = TERM_ROWS - 1;
            }
        } else if (c == '\b') {
            if (term_col > 0) {
                term_col--;
                term_buffer[term_row][term_col] = ' ';
            }
        } else {
            if (term_col < TERM_COLS) {
                term_buffer[term_row][term_col] = c;
                term_col++;
            } else {
                term_col = 0;
                term_row++;
                if (term_row >= TERM_ROWS) {
                    for (int r = 0; r < TERM_ROWS - 1; r++) {
                        for (int col = 0; col < TERM_COLS; col++) {
                            term_buffer[r][col] = term_buffer[r + 1][col];
                        }
                        term_buffer[r][TERM_COLS] = '\0';
                    }
                    for (int col = 0; col < TERM_COLS; col++) {
                        term_buffer[TERM_ROWS - 1][col] = ' ';
                    }
                    term_buffer[TERM_ROWS - 1][TERM_COLS] = '\0';
                    term_row = TERM_ROWS - 1;
                }
                term_buffer[term_row][term_col] = c;
                term_col++;
            }
        }
    }
}

void wm_select_next_window(void) {
    if (window_count <= 1) return;
    windows[active_window_idx].active = 0;
    active_window_idx = (active_window_idx + 1) % window_count;

    int attempts = 0;
    while (!windows[active_window_idx].visible && attempts < window_count) {
        active_window_idx = (active_window_idx + 1) % window_count;
        attempts++;
    }
    windows[active_window_idx].active = 1;
}

void wm_move_cursor(int dx, int dy) {
    mouse_x += dx;
    mouse_y += dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= (int)sys_fb.framebuffer_width) mouse_x = sys_fb.framebuffer_width - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= (int)sys_fb.framebuffer_height) mouse_y = sys_fb.framebuffer_height - 1;
}

void wm_move_active_window(int dx, int dy) {
    if (window_count == 0) return;
    windows[active_window_idx].x += dx;
    windows[active_window_idx].y += dy;
}

/* --- Вспомогательный рендеринг примитивов с клиппингом --- */
static void render_rect_clipped(int win_x, int win_y, int win_w, int win_h, int rel_x, int rel_y, int w, int h, uint32_t color) {
    int abs_x = win_x + rel_x;
    int abs_y = win_y + 22 + rel_y;
    int max_x = win_x + win_w;
    int max_y = win_y + win_h;
    int min_y = win_y + 22;

    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            int px = abs_x + c;
            int py = abs_y + r;
            if (px >= win_x && px < max_x && py >= min_y && py < max_y) {
                draw_pixel((uint32_t)px, (uint32_t)py, color);
            }
        }
    }
}

static void render_button_clipped(int win_x, int win_y, int win_w, int win_h, int rel_x, int rel_y, int w, int h, const char* text, uint32_t bg_color) {
    render_rect_clipped(win_x, win_y, win_w, win_h, rel_x, rel_y, w, h, bg_color);
    render_rect_clipped(win_x, win_y, win_w, win_h, rel_x, rel_y, w, 1, 0x61AFEF);
    render_rect_clipped(win_x, win_y, win_w, win_h, rel_x, rel_y + h - 1, w, 1, 0x61AFEF);
    render_rect_clipped(win_x, win_y, win_w, win_h, rel_x, rel_y, 1, h, 0x61AFEF);
    render_rect_clipped(win_x, win_y, win_w, win_h, rel_x + w - 1, rel_y, 1, h, 0x61AFEF);

    int abs_x = win_x + rel_x + 8;
    int abs_y = win_y + 22 + rel_y + (h / 2) - 4;
    print_at(text, abs_x, abs_y, 0xFFFFFF);
}

/* --- Системные вызовы отрисовки с сохранением в буфер окна --- */
void wm_draw_window_rect(int win_id, int rel_x, int rel_y, int w, int h, uint32_t color) {
    if (win_id < 0 || win_id >= window_count || !windows[win_id].visible) return;

    int found = -1;
    for (int k = 0; k < windows[win_id].cmd_count; k++) {
        if (windows[win_id].cmds[k].type == CMD_RECT &&
            windows[win_id].cmds[k].x == rel_x &&
            windows[win_id].cmds[k].y == rel_y) {
            found = k;
            break;
        }
    }
    int idx = (found != -1) ? found : windows[win_id].cmd_count;
    if (found == -1 && windows[win_id].cmd_count < MAX_WINDOW_CMDS) {
        windows[win_id].cmd_count++;
    }

    if (idx < MAX_WINDOW_CMDS) {
        windows[win_id].cmds[idx].type = CMD_RECT;
        windows[win_id].cmds[idx].x = rel_x;
        windows[win_id].cmds[idx].y = rel_y;
        windows[win_id].cmds[idx].w = w;
        windows[win_id].cmds[idx].h = h;
        windows[win_id].cmds[idx].color = color;
    }
}

void wm_draw_window_text(int win_id, int rel_x, int rel_y, const char* str, uint32_t color) {
    if (win_id < 0 || win_id >= window_count || !windows[win_id].visible || !str) return;

    int found = -1;
    for (int k = 0; k < windows[win_id].cmd_count; k++) {
        if (windows[win_id].cmds[k].type == CMD_TEXT &&
            windows[win_id].cmds[k].x == rel_x &&
            windows[win_id].cmds[k].y == rel_y) {
            found = k;
            break;
        }
    }
    int idx = (found != -1) ? found : windows[win_id].cmd_count;
    if (found == -1 && windows[win_id].cmd_count < MAX_WINDOW_CMDS) {
        windows[win_id].cmd_count++;
    }

    if (idx < MAX_WINDOW_CMDS) {
        windows[win_id].cmds[idx].type = CMD_TEXT;
        windows[win_id].cmds[idx].x = rel_x;
        windows[win_id].cmds[idx].y = rel_y;
        windows[win_id].cmds[idx].color = color;
        int i = 0;
        while (str[i] != '\0' && i < 63) {
            windows[win_id].cmds[idx].text[i] = str[i];
            i++;
        }
        windows[win_id].cmds[idx].text[i] = '\0';
    }
}

void wm_draw_window_button(int win_id, int rel_x, int rel_y, int w, int h, const char* text, uint32_t bg_color) {
    if (win_id < 0 || win_id >= window_count || !windows[win_id].visible) return;

    int found = -1;
    for (int k = 0; k < windows[win_id].cmd_count; k++) {
        if (windows[win_id].cmds[k].type == CMD_BUTTON &&
            windows[win_id].cmds[k].x == rel_x &&
            windows[win_id].cmds[k].y == rel_y) {
            found = k;
            break;
        }
    }
    int idx = (found != -1) ? found : windows[win_id].cmd_count;
    if (found == -1 && windows[win_id].cmd_count < MAX_WINDOW_CMDS) {
        windows[win_id].cmd_count++;
    }

    if (idx < MAX_WINDOW_CMDS) {
        windows[win_id].cmds[idx].type = CMD_BUTTON;
        windows[win_id].cmds[idx].x = rel_x;
        windows[win_id].cmds[idx].y = rel_y;
        windows[win_id].cmds[idx].w = w;
        windows[win_id].cmds[idx].h = h;
        windows[win_id].cmds[idx].color = bg_color;
        int i = 0;
        if (text) {
            while (text[i] != '\0' && i < 63) {
                windows[win_id].cmds[idx].text[i] = text[i];
                i++;
            }
        }
        windows[win_id].cmds[idx].text[i] = '\0';
    }
}

int wm_poll_event(int win_id, struct gui_event* ev) {
    if (!ev || win_id < 0 || win_id >= window_count || !windows[win_id].visible) return 0;

    if (win_id != active_window_idx) {
        ev->type = EVENT_NONE;
        return 0;
    }

    char c = wm_pop_key();
    if (c != 0) {
        ev->type = EVENT_KEY_PRESS;
        ev->param1 = (int32_t)c;
        ev->param2 = 0;
        ev->param3 = 0;
        return 1;
    }

    ev->type = EVENT_NONE;
    return 0;
}

void wm_draw_scene(void) {
    uint32_t sw = sys_fb.framebuffer_width ? sys_fb.framebuffer_width : 1024;
    uint32_t sh = sys_fb.framebuffer_height ? sys_fb.framebuffer_height : 768;

    // 1. Отрисовка фона
    wm_draw_background();

    // 2. Верхняя системная панель
    fill_rect(0, 0, sw, 24, 0x1A1F2C);
    fill_rect(0, 24, sw, 1, 0x2D3748);
    print_at(" MIGHT OS 1.1 ", 8, 4, 0x61AFEF);
    print_at("[ALT+TAB] Switch | [ALT+F4] Close | [ALT+Arrows] Move", 180, 4, 0xABB2BF);

    // 3. Оконный менеджер (двухпроходная отрисовка: сначала неактивные, затем активное окно сверху)
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < window_count; i++) {
            if (!windows[i].visible) continue;
            int is_active = (i == active_window_idx);

            if ((pass == 0 && is_active) || (pass == 1 && !is_active)) continue;

            int wx = windows[i].x;
            int wy = windows[i].y;
            int ww = windows[i].width;
            int wh = windows[i].height;

            // Тень окна
            fill_rect(wx + 4, wy + 4, ww, wh, 0x080A0F);

            // Заголовок окна (Titlebar)
            uint32_t title_bg = is_active ? 0x282C34 : 0x1E222A;
            uint32_t title_fg = is_active ? 0x61AFEF : 0x5C6370;
            fill_rect(wx, wy, ww, 22, title_bg);
            print_at(windows[i].title, wx + 8, wy + 3, title_fg);
            print_at("X", wx + ww - 16, wy + 3, 0xE06C75);

            // Рабочая область окна
            fill_rect(wx, wy + 22, ww, wh - 22, windows[i].bg_color);
            draw_window(wx, wy, ww, wh, is_active ? 0x61AFEF : 0x3E4451);

            // Окно 0: Системный Терминал
            if (i == 0) {
                int tx = wx + 8;
                int ty = wy + 28;
                for (int r = 0; r < TERM_ROWS; r++) {
                    if (ty + 16 > wy + wh - 4) break;
                    print_at(term_buffer[r], tx, ty, 0x98C379);
                    ty += 16;
                }
                int cx = tx + (term_col * FONT_W);
                int cy = wy + 28 + (term_row * 16);
                if (cx < wx + ww - 8 && cy < wy + wh - 4) {
                    fill_rect(cx, cy, 8, 14, 0x98C379);
                }
            }
            // Окно 1: System Info
            else if (i == 1) {
                int tx = wx + 12;
                int ty = wy + 30;
                print_at("OS: MIGHT OS 1.1 x86_64", tx, ty, 0xE5C07B);
                print_at("Kernel: Freestanding C", tx, ty + 20, 0xABB2BF);
                print_at("GUI: Custom LFB WM", tx, ty + 40, 0xABB2BF);
                print_at("RAM: 256 MB / QEMU", tx, ty + 60, 0x98C379);
                print_at("FS: MIGHT FS Raw", tx, ty + 80, 0x61AFEF);

                print_hy_at("Բարի գալուստ MIGHT OS", tx, ty + 105, 0x61AFEF);
            }
            // Пользовательские GUI-окна (i >= 2): Автоматический рендеринг сохраненных команд
            else if (windows[i].custom_gui) {
                for (int c = 0; c < windows[i].cmd_count; c++) {
                    draw_cmd_t *cmd = &windows[i].cmds[c];
                    if (cmd->type == CMD_RECT) {
                        render_rect_clipped(wx, wy, ww, wh, cmd->x, cmd->y, cmd->w, cmd->h, cmd->color);
                    } else if (cmd->type == CMD_TEXT) {
                        int abs_x = wx + cmd->x;
                        int abs_y = wy + 22 + cmd->y;
                        print_at(cmd->text, abs_x, abs_y, cmd->color);
                    } else if (cmd->type == CMD_BUTTON) {
                        render_button_clipped(wx, wy, ww, wh, cmd->x, cmd->y, cmd->w, cmd->h, cmd->text, cmd->color);
                    }
                }
            }
        }
    }

    // 4. Отрисовка стрелочного курсора мыши (12x12 пикселей)
    static const uint16_t cursor_sprite[12] = {
        0b100000000000,
        0b110000000000,
        0b111000000000,
        0b111100000000,
        0b111110000000,
        0b111111000000,
        0b111111100000,
        0b111110000000,
        0b100111000000,
        0b000011100000,
        0b000001110000,
        0b000000110000
    };

    for (int cy = 0; cy < 12; cy++) {
        for (int cx = 0; cx < 12; cx++) {
            if (cursor_sprite[cy] & (1 << (11 - cx))) {
                draw_pixel(mouse_x + cx, mouse_y + cy, 0xE06C75);
            }
        }
    }
}

#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <stdint.h>

#define EVENT_NONE        0
#define EVENT_MOUSE_CLICK 1
#define EVENT_KEY_PRESS   2

struct gui_event {
    uint32_t type;     // Тип события
    int32_t  param1;   // X rel / ASCII key
    int32_t  param2;   // Y rel
    uint32_t param3;   // Флаги (state)
};

void wm_init(void);
int  wm_create_window(int x, int y, int w, int h, const char* title, uint32_t color);
void wm_destroy_window(int win_id);
void wm_destroy_active_window(void);
void wm_draw_scene(void);
void wm_terminal_print(const char* str);
void wm_terminal_clear(void);
void wm_select_next_window(void);
void wm_move_cursor(int dx, int dy);
void wm_move_active_window(int dx, int dy);
void wm_push_key(char c);

/* API для пользовательских приложений (Syscalls GUI) */
void wm_draw_window_rect(int win_id, int rel_x, int rel_y, int w, int h, uint32_t color);
void wm_draw_window_text(int win_id, int rel_x, int rel_y, const char* str, uint32_t color);
void wm_draw_window_button(int win_id, int rel_x, int rel_y, int w, int h, const char* text, uint32_t bg_color);
int  wm_poll_event(int win_id, struct gui_event* ev);

#endif

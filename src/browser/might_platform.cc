#include "might_bridge.h"

static syscall_t g_sys = nullptr;
static uint64_t g_win_id = 0;

extern "C" void dillo_set_might_context(syscall_t sys, uint64_t win_id) {
    g_sys = sys;
    g_win_id = win_id;
}

extern "C" void dillo_draw_rectangle(int x, int y, int w, int h, uint32_t color) {
    if (!g_sys) return;
    // 6 аргументов: SYS_DRAW_RECT, win_id, x, y, w, (color << 32 | h)
    g_sys(SYS_DRAW_RECT, g_win_id, (uint64_t)x, (uint64_t)y, (uint64_t)w,
          ((uint64_t)color << 32) | (uint64_t)(h & 0xFFFF));
}

extern "C" void dillo_draw_string(int x, int y, const char* text, uint32_t color) {
    if (!g_sys || !text) return;
    // 6 аргументов: SYS_DRAW_TEXT, win_id, x, y, text, color
    g_sys(SYS_DRAW_TEXT, g_win_id, (uint64_t)x, (uint64_t)y, (uint64_t)text, (uint64_t)color);
}

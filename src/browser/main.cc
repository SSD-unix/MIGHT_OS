#include "might_bridge.h"

extern "C" void dillo_set_might_context(syscall_t sys, uint64_t win_id);
extern "C" void dillo_draw_rectangle(int x, int y, int w, int h, uint32_t color);
extern "C" void dillo_draw_string(int x, int y, const char* text, uint32_t color);

/* Микро-парсер тегов на базе алгоритма Dillo */
static void parse_and_render_html(const char* html) {
    int cursor_x = 20;
    int cursor_y = 60;

    // Очищаем область контента страницы
    dillo_draw_rectangle(10, 45, 620, 385, 0x1E222A);

    const char* p = html;
    char text_buf[256];
    int buf_idx = 0;
    uint32_t text_color = 0xABB2BF;

    while (*p) {
        if (*p == '<') {
            if (buf_idx > 0) {
                text_buf[buf_idx] = '\0';
                dillo_draw_string(cursor_x, cursor_y, text_buf, text_color);
                buf_idx = 0;
            }

            if (p[1] == 'h' && p[2] == '1') { // <h1> Header
                cursor_y += 24;
                cursor_x = 20;
                text_color = 0x61AFEF;
                p += 4;
            } else if (p[1] == 'p') { // <p> Paragraph
                cursor_y += 18;
                cursor_x = 20;
                text_color = 0xABB2BF;
                p += 3;
            } else if (p[1] == 'a') { // <a href> Link
                text_color = 0x98C379;
                while (*p && *p != '>') p++;
                if (*p) p++;
            } else {
                while (*p && *p != '>') p++;
                if (*p) p++;
            }
        } else if (*p == '\n' || *p == '\r') {
            p++;
        } else {
            if (buf_idx < 255) {
                text_buf[buf_idx++] = *p;
            }
            p++;
        }
    }

    if (buf_idx > 0) {
        text_buf[buf_idx] = '\0';
        dillo_draw_string(cursor_x, cursor_y, text_buf, text_color);
    }
}

extern "C" void _start(void* kernel_dispatcher) {
    syscall_t sys = (syscall_t)kernel_dispatcher;

    uint64_t win_id = sys(SYS_CREATE_WINDOW, (uint64_t)"Dillo HTML Engine (MIGHT OS)", 640, 440, 70, 50);
    dillo_set_might_context(sys, win_id);

    // 1. HTTP GET запрос
    sys(SYS_HTTP_GET, (uint64_t)"10.0.2.2", (uint64_t)"/", 0, 0, 0);

    // 2. Отрисовка шапки браузера Dillo
    dillo_draw_rectangle(10, 10, 620, 26, 0x282C34);
    dillo_draw_string(20, 15, "URL: http://10.0.2.2/ | Dillo Layout Engine", 0x61AFEF);

    // Тестовая страница для рендеринга Dillo
    const char* test_html = "<h1>Welcome to MIGHT OS</h1><p>Dillo HTML Engine running without glibc.</p><a href='#'>Clickable Link</a>";
    parse_and_render_html(test_html);

    sys(SYS_REDRAW_WIN, 0, 0, 0, 0, 0);

    struct {
        int32_t type;
        int32_t param1;
        int32_t param2;
        int32_t param3;
    } ev;

    while (1) {
        uint64_t res = sys(SYS_POLL_EVENT, win_id, (uint64_t)&ev, 0, 0, 0);
        if (res > 0 && ev.param1 == 13) {
            sys(SYS_HTTP_GET, (uint64_t)"10.0.2.2", (uint64_t)"/", 0, 0, 0);
        }
        sys(SYS_REDRAW_WIN, 0, 0, 0, 0, 0);
    }
}

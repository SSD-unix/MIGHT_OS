#ifndef MIGHT_BRIDGE_H
#define MIGHT_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#define SYS_CREATE_WINDOW  10
#define SYS_DRAW_RECT      11
#define SYS_DRAW_TEXT      12
#define SYS_DRAW_BUTTON    13
#define SYS_POLL_EVENT     14
#define SYS_REDRAW_WIN     15
#define SYS_HTTP_GET       20
#define SYS_EXIT           60

typedef uint64_t (*syscall_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/* 4MB Арена памяти для бинарника без POSIX libc */
static char might_heap[4 * 1024 * 1024];
static size_t heap_ptr = 0;

static inline void* dillo_alloc(size_t size) {
    if (heap_ptr + size >= sizeof(might_heap)) return 0;
    void* p = &might_heap[heap_ptr];
    heap_ptr = (heap_ptr + size + 7) & ~7; // Выравнивание по 8 байт
    return p;
}

static inline void dillo_free(void* p) { (void)p; }

#endif

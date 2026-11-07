#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define MAX_TASKS 16
#define TASK_STACK_SIZE 65536 // 64 KB стека на задачу

typedef enum {
    TASK_STATE_FREE = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_DEAD
} task_state_t;

// Контекст регистров x86_64, сохраняемый при переключении
typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rflags;
    uint64_t rip;
} cpu_context_t;

typedef struct task {
    int id;
    task_state_t state;
    uint64_t rsp;                           // Указатель стека задачи
    uint8_t stack[TASK_STACK_SIZE] __attribute__((aligned(16)));
    int win_id;                             // ID окна в Window Manager (-1 если нет)
    char name[32];
} task_t;

void task_init(void);
int task_create(void (*entry_point)(void*), void* arg, const char* name);
void task_exit(void);
void schedule(void);
task_t* task_get_current(void);

extern void cpu_context_switch(uint64_t* old_rsp, uint64_t new_rsp);

#endif

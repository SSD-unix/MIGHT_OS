/* kernel/task.c - MIGHT OS Task Scheduler */
#include "task.h"
#include "../drivers/window_manager.h"

extern void task_trampoline_asm(void);

static task_t tasks[MAX_TASKS];
static int current_task_idx = 0;

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_STATE_FREE;
        tasks[i].id = i;
        tasks[i].win_id = -1;
    }

    // Задача 0 — Ядро / Главный поток
    tasks[0].state = TASK_STATE_RUNNING;
    tasks[0].win_id = 0;
    current_task_idx = 0;
}

task_t* task_get_current(void) {
    return &tasks[current_task_idx];
}

int task_create(void (*entry_point)(void*), void* arg, const char* name) {
    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_STATE_FREE || tasks[i].state == TASK_STATE_DEAD) {
            slot = i;
            break;
        }
    }

    if (slot == -1) return -1;

    task_t* t = &tasks[slot];
    t->state = TASK_STATE_READY;
    t->win_id = -1;

    int n = 0;
    if (name) {
        while (name[n] != '\0' && n < 31) {
            t->name[n] = name[n];
            n++;
        }
    }
    t->name[n] = '\0';

    uint64_t* sp = (uint64_t*)&t->stack[TASK_STACK_SIZE];
    sp = (uint64_t*)((uintptr_t)sp & ~0x0FULL);

    // Укладываем начальное состояние стека для cpu_context_switch
    *(--sp) = (uint64_t)task_trampoline_asm; // RIP после ret
    *(--sp) = 0x202;                         // RFLAGS (IF=1)
    *(--sp) = 0;                             // RBP
    *(--sp) = 0;                             // RBX
    *(--sp) = 0;                             // R12
    *(--sp) = (uint64_t)arg;                 // R13 -> передаем dispatcher_ptr
    *(--sp) = (uint64_t)entry_point;         // R14 -> передаем entry_point
    *(--sp) = 0;                             // R15

    t->rsp = (uint64_t)sp;
    return slot;
}

void task_exit(void) {
    tasks[current_task_idx].state = TASK_STATE_DEAD;
    schedule();
}

void schedule(void) {
    int prev_idx = current_task_idx;
    int next_idx = (current_task_idx + 1) % MAX_TASKS;

    while (next_idx != current_task_idx) {
        if (tasks[next_idx].state == TASK_STATE_READY || tasks[next_idx].state == TASK_STATE_RUNNING) {
            break;
        }
        next_idx = (next_idx + 1) % MAX_TASKS;
    }

    if (next_idx == current_task_idx && tasks[current_task_idx].state != TASK_STATE_RUNNING) {
        next_idx = 0;
    }

    if (next_idx == prev_idx) return;

    if (tasks[prev_idx].state == TASK_STATE_RUNNING) {
        tasks[prev_idx].state = TASK_STATE_READY;
    }

    current_task_idx = next_idx;
    tasks[current_task_idx].state = TASK_STATE_RUNNING;

    cpu_context_switch(&tasks[prev_idx].rsp, tasks[next_idx].rsp);
}

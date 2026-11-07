#include "might_bridge.h"

void* operator new(size_t size) { return dillo_alloc(size); }
void* operator new[](size_t size) { return dillo_alloc(size); }
void operator delete(void* p) noexcept { (void)p; }
void operator delete[](void* p) noexcept { (void)p; }
void operator delete(void* p, size_t size) noexcept { (void)p; (void)size; }
void operator delete[](void* p, size_t size) noexcept { (void)p; (void)size; }

extern "C" void __cxa_pure_virtual() { while (1) {} }

/* Заглушка защиты стека GCC */
extern "C" void __stack_chk_fail() {
    while (1) {}
}
uintptr_t __stack_chk_guard = 0x595e9fbd94fda766;

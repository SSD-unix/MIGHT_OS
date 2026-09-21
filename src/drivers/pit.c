#include "pit.h"
#include "lowlevel_io.h"

#define PIT_FREQUENCY 1193180
#define PIT_CHANNEL0  0x40
#define PIT_COMMAND   0x43

static volatile uint32_t tick_counter = 0;

void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;
    if (divisor > 65535) divisor = 65535;
    if (divisor < 1) divisor = 1;

    port_byte_out(PIT_COMMAND, 0x36);
    port_byte_out(PIT_CHANNEL0, divisor & 0xFF);
    port_byte_out(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

uint32_t pit_get_ticks(void) {
    return tick_counter;
}

void pit_irq_handler(void) {
    tick_counter++;
    port_byte_out(0x20, 0x20);  // EOI
}

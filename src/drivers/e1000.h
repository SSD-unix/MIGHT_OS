/* drivers/e1000.h */
#ifndef E1000_H
#define E1000_H

#include "../common.h"

void e1000_init();
void e1000_send_packet(uint8_t *data, uint16_t length);
void ping_host(char* host);

#endif

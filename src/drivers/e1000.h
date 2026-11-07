#ifndef E1000_H
#define E1000_H

#include "../common.h"

extern uint8_t my_mac[6];
extern uint8_t my_ip[4];

void e1000_init(void);
void e1000_send_packet(uint8_t *data, uint16_t length);
int e1000_poll_packet(uint8_t *out_buffer, uint16_t *out_len);
void ping_host(char* host);
uint16_t net_checksum(uint8_t *addr, int count);

#endif

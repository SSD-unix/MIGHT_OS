/* net/netif.c */
#include "../drivers/e1000.h"
#include "../drivers/screen.h"

// Функция отправки пакета, которую требует lwIP
void low_level_output(uint8_t *buffer, uint16_t len) {
    e1000_send_packet(buffer, len);
}

// Вызывается при получении сырого пакета от сетевой карты
void netif_input_packet(uint8_t *buffer, uint16_t len) {
    kprint("[NET] Packet received. Forwarding to lwIP stack...\n");
    // Передача пакета в lwip_input()
}

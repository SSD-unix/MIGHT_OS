#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "../drivers/e1000.h"
#include "../drivers/screen.h"

// Низкоуровневая отправка кадра из lwIP в e1000
static err_t e1000_low_level_output(struct netif *netif, struct pbuf *p) {
    uint8_t buffer[2048];
    uint16_t len = 0;

    // Копируем данные из цепочки pbuf lwIP в плоский буфер
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        if (len + q->len > 2048) break;
        for (u16_t i = 0; i < q->len; i++) {
            buffer[len++] = ((uint8_t*)q->payload)[i];
        }
    }

    // Отправляем через наш работающий DMA драйвер e1000
    e1000_send_packet(buffer, len);
    return ERR_OK;
}

// Инициализация сетевого интерфейса lwIP
err_t e1000_netif_init(struct netif *netif) {
    netif->name[0] = 'e';
    netif->name[1] = '0';
    netif->output = etharp_output; // Сборка ARP + IP
    netif->linkoutput = e1000_low_level_output; // Физическая отправка
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    // Задаем MAC адрес карты
    netif->hwaddr_len = 6;
    for (int i = 0; i < 6; i++) {
        netif->hwaddr[i] = my_mac[i];
    }

    return ERR_OK;
}

// Опрос входящих пакетов и передача в lwIP
void e1000_netif_poll(struct netif *netif) {
    uint8_t rx_buf[2048];
    uint16_t rx_len = 0;

    while (e1000_poll_packet(rx_buf, &rx_len)) {
        if (rx_len == 0) continue;

        // Выделяем pbuf буфер в lwIP
        struct pbuf *p = pbuf_alloc(PBUF_RAW, rx_len, PBUF_POOL);
        if (p != NULL) {
            pbuf_take(p, rx_buf, rx_len);
            // Передаем пакет на обработку в стек lwIP (Ethernet / ARP / IP / TCP)
            if (netif->input(p, netif) != ERR_OK) {
                pbuf_free(p);
            }
        }
    }
}

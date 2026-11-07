#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "../drivers/screen.h"

static err_t wget_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // Сервер закрыл соединение
        kprint("\n[WGET] Download complete! Connection closed.\n");
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Выводим полученные данные (HTTP заголовки + тело файла) в терминал MIGHT OS
    char *data = (char*)p->payload;
    for (u16_t i = 0; i < p->len; i++) {
        char str[2] = {data[i], '\0'};
        kprint(str);
    }

    // Сообщаем lwIP, что обработали N байт (для TCP Window ACK)
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t wget_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    kprint("[WGET] Connected to HTTP server! Sending GET request...\n");

    // Формируем HTTP GET запрос
    char *http_request = "GET / HTTP/1.1\r\nHost: 10.0.2.2\r\nUser-Agent: MIGHT_OS\r\nConnection: close\r\n\r\n";

    tcp_write(tpcb, http_request, 70, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb); // Отправляем TCP SYN / PUSH кадр

    return ERR_OK;
}

// Запуск скачивания файла по IP (например, 10.0.2.2 или 93.184.216.34)
void http_wget(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint16_t port) {
    kprint("[WGET] Connecting to server...\n");

    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        kprint("[WGET] Failed to create TCP PCB!\n");
        return;
    }

    ip_addr_t server_ip;
    IP4_ADDR(&server_ip, ip0, ip1, ip2, ip3);

    // Вешаем коллбэки
    tcp_recv(pcb, wget_recv_callback);
    // Открываем TCP 3-way handshake (SYN)
    tcp_connect(pcb, &server_ip, port, wget_connected_callback);
}

/* drivers/wget.c — Полноценный микро-клиент HTTP/TCP для MIGHT OS */
#include "wget.h"
#include "e1000.h"
#include "screen.h"
#include "../kernel/utils.h"

extern uint8_t gateway_mac[6];
extern uint8_t my_mac[6];
extern uint8_t my_ip[4];
extern uint16_t net_checksum(uint8_t *addr, int count);
extern void kprint_int(int n);

/* Переворот байт для Network Byte Order (Big-Endian) */
static inline uint16_t htons(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static inline uint32_t htonl(uint32_t val) {
    return __builtin_bswap32(val);
}

static inline uint32_t ntohl(uint32_t val) {
    return __builtin_bswap32(val);
}

/* Вычисление обязательной контрольной суммы TCP с Псевдо-Заголовком (RFC 793) */
static uint16_t tcp_checksum(uint8_t *src_ip, uint8_t *dest_ip, uint8_t *tcp_seg, uint16_t tcp_len) {
    uint32_t sum = 0;

    // 1. Pseudo-Header (12 байт)
    sum += ((uint16_t)src_ip[0] << 8) | src_ip[1];
    sum += ((uint16_t)src_ip[2] << 8) | src_ip[3];
    sum += ((uint16_t)dest_ip[0] << 8) | dest_ip[1];
    sum += ((uint16_t)dest_ip[2] << 8) | dest_ip[3];
    sum += 0x0006; // Protocol TCP (6)
    sum += tcp_len;

    // 2. TCP Header + Payload
    for (int i = 0; i < tcp_len - 1; i += 2) {
        sum += ((uint16_t)tcp_seg[i] << 8) | tcp_seg[i + 1];
    }
    if (tcp_len & 1) {
        sum += ((uint16_t)tcp_seg[tcp_len - 1] << 8);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

static int parse_ip(const char* ip_str, uint8_t* ip_out) {
    int val = 0, idx = 0;
    for (int i = 0; ; i++) {
        if (ip_str[i] >= '0' && ip_str[i] <= '9') {
            val = val * 10 + (ip_str[i] - '0');
        } else if (ip_str[i] == '.' || ip_str[i] == '\0') {
            if (val > 255 || idx >= 4) return 0;
            ip_out[idx++] = (uint8_t)val;
            val = 0;
            if (ip_str[i] == '\0') break;
        } else {
            return 0;
        }
    }
    return (idx == 4);
}

void wget_download(const char* ip_str, const char* path) {
    uint8_t dest_ip[4];
    if (!parse_ip(ip_str, dest_ip)) {
        kprint("Usage: WGET <IP> <path>\nExample: WGET 10.0.2.2 /\n");
        return;
    }

    if (!path || path[0] == '\0') path = "/";

    kprint("Connecting to HTTP server ");
    kprint(ip_str);
    kprint(":80 via TCP Handshake...\n");

    uint8_t frame[512];
    uint16_t src_port = 49152 + (my_ip[3] & 0x0F);
    uint32_t client_seq = 0x10203040;

    /* =======================================================
     * ШАГ 1: Отправка TCP SYN (Установка соединения)
     * ======================================================= */
    // Ethernet Header
    for (int i = 0; i < 6; i++) frame[i] = gateway_mac[i];
    for (int i = 0; i < 6; i++) frame[6 + i] = my_mac[i];
    frame[12] = 0x08; frame[13] = 0x00; // IPv4

    // IP Header (20 байт)
    frame[14] = 0x45; frame[15] = 0x00;
    frame[16] = 0x00; frame[17] = 0x28; // Total Length = 40 (20 IP + 20 TCP)
    frame[18] = 0x2A; frame[19] = 0x11; // ID
    frame[20] = 0x00; frame[21] = 0x00;
    frame[22] = 0x40;                   // TTL = 64
    frame[23] = 0x06;                   // Protocol = TCP
    frame[24] = 0x00; frame[25] = 0x00;
    for (int i = 0; i < 4; i++) frame[26 + i] = my_ip[i];
    for (int i = 0; i < 4; i++) frame[30 + i] = dest_ip[i];

    uint16_t ip_sum = net_checksum(&frame[14], 20);
    frame[24] = (ip_sum >> 8) & 0xFF;
    frame[25] = ip_sum & 0xFF;

    // TCP Header (20 байт)
    *(uint16_t*)&frame[34] = htons(src_port);
    *(uint16_t*)&frame[36] = htons(80);
    *(uint32_t*)&frame[38] = htonl(client_seq);
    *(uint32_t*)&frame[42] = 0; // ACK = 0
    frame[46] = 0x50;           // Offset = 5 (20 байт)
    frame[47] = 0x02;           // Flags = SYN (0x02)
    *(uint16_t*)&frame[48] = htons(64240); // Window Size
    frame[50] = 0x00; frame[51] = 0x00;
    *(uint16_t*)&frame[52] = 0;

    // TCP Checksum SYN
    uint16_t tcp_sum = tcp_checksum(my_ip, dest_ip, &frame[34], 20);
    frame[50] = (tcp_sum >> 8) & 0xFF;
    frame[51] = tcp_sum & 0xFF;

    e1000_send_packet(frame, 54); // 14 + 40

    /* =======================================================
     * ШАГ 2: Ожидание TCP SYN-ACK от сервера
     * ======================================================= */
    uint8_t rx_buf[2048];
    uint16_t rx_len = 0;
    uint32_t server_seq = 0;
    int syn_ack_received = 0;

    for (volatile uint32_t timeout = 0; timeout < 5000000; timeout++) {
        if (e1000_poll_packet(rx_buf, &rx_len)) {
            // Ищем IPv4 (0x0800), TCP (0x06), Dst Port == src_port
            if (rx_len >= 54 && rx_buf[12] == 0x08 && rx_buf[13] == 0x00 && rx_buf[23] == 0x06) {
                uint16_t dport = (rx_buf[36] << 8) | rx_buf[37];
                uint8_t flags = rx_buf[47];

                if (dport == src_port && (flags & 0x12) == 0x12) { // SYN + ACK (0x12)
                    server_seq = ntohl(*(uint32_t*)&rx_buf[38]);
                    syn_ack_received = 1;
                    break;
                }
            }
        }
    }

    if (!syn_ack_received) {
        kprint("WGET Error: Connection timeout (No SYN-ACK from server).\n");
        return;
    }

    kprint("TCP Handshake Established! Sending HTTP GET request...\n");

    /* =======================================================
     * ШАГ 3: Отправка PSH + ACK с HTTP GET запросом
     * ======================================================= */
    client_seq++;
    uint32_t ack_seq = server_seq + 1;

    char req_buf[256];
    int req_len = 0;

    // Формируем GET
    req_buf[req_len++] = 'G'; req_buf[req_len++] = 'E'; req_buf[req_len++] = 'T'; req_buf[req_len++] = ' ';
    for (int i = 0; path[i] != '\0' && req_len < 120; i++) req_buf[req_len++] = path[i];

    const char* req_tail = " HTTP/1.1\r\nHost: 10.0.2.2\r\nUser-Agent: MIGHT_OS\r\nConnection: close\r\n\r\n";
    for (int i = 0; req_tail[i] != '\0'; i++) req_buf[req_len++] = req_tail[i];

    uint16_t tcp_total_len = 20 + req_len;
    uint16_t ip_total_len = 20 + tcp_total_len;

    // Обновляем IP Header
    frame[16] = (ip_total_len >> 8) & 0xFF;
    frame[17] = ip_total_len & 0xFF;
    frame[24] = 0x00; frame[25] = 0x00;
    ip_sum = net_checksum(&frame[14], 20);
    frame[24] = (ip_sum >> 8) & 0xFF;
    frame[25] = ip_sum & 0xFF;

    // Обновляем TCP Header
    *(uint32_t*)&frame[38] = htonl(client_seq);
    *(uint32_t*)&frame[42] = htonl(ack_seq);
    frame[47] = 0x18; // Flags: PSH + ACK (0x18)
    frame[50] = 0x00; frame[51] = 0x00;

    // Копируем payload HTTP
    for (int i = 0; i < req_len; i++) {
        frame[54 + i] = req_buf[i];
    }

    // Пересчитываем TCP Checksum
    tcp_sum = tcp_checksum(my_ip, dest_ip, &frame[34], tcp_total_len);
    frame[50] = (tcp_sum >> 8) & 0xFF;
    frame[51] = tcp_sum & 0xFF;

    e1000_send_packet(frame, 14 + ip_total_len);

    /* =======================================================
         * ШАГ 4: Прием и вывод всех сегментов HTTP (Payload)
         * ======================================================= */
        kprint("\n--- HTTP RESPONSE ---\n");
        int received_any = 0;

        // Крутим опрос до тех пор, пока идут пакеты данных от сервера
        for (volatile uint32_t timeout = 0; timeout < 15000000; timeout++) {
            if (e1000_poll_packet(rx_buf, &rx_len)) {
                // Проверяем: IPv4 (0x0800) и TCP Protocol (0x06)
                if (rx_len >= 54 && rx_buf[12] == 0x08 && rx_buf[13] == 0x00 && rx_buf[23] == 0x06) {
                    uint16_t src_p = (rx_buf[34] << 8) | rx_buf[35];
                    uint16_t dst_p = (rx_buf[36] << 8) | rx_buf[37];

                    // Наш порт и ответ от порта 80
                    if (src_p == 80 && dst_p == src_port) {
                        uint8_t tcp_header_len = (rx_buf[46] >> 4) * 4;
                        uint16_t payload_offset = 14 + 20 + tcp_header_len;

                        // Если в пакете есть полезные данные HTTP
                        if (rx_len > payload_offset) {
                            for (int i = payload_offset; i < rx_len; i++) {
                                char str[2] = {rx_buf[i], '\0'};
                                kprint(str);
                            }
                            received_any = 1;
                        }

                        // Если сервер прислал FIN (0x01 или 0x11), завершаем прием
                        uint8_t flags = rx_buf[47];
                        if (flags & 0x01) {
                            break;
                        }

                        // Сбрасываем таймаут при каждом полученном пакете, чтобы дождаться следующих сегментов
                        timeout = 0;
                    }
                }
            }
        }

        kprint("\n---------------------\n");

        if (!received_any) {
            kprint("WGET Error: Server accepted connection, but payload was empty or dropped.\n");
        }
}

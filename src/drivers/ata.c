/* drivers/ata.c */
#include "ata.h"
#include "lowlevel_io.h"
#include "screen.h"
#include "../common.h"

// --- Аппаратные функции для прямой работы процессора с памятью ---
static inline void insw(uint16_t port, void *addr, uint32_t word_count) {
    __asm__ volatile("rep insw" : "+D"(addr), "+c"(word_count) : "d"(port) : "memory");
}

static inline void outsw(uint16_t port, const void *addr, uint32_t word_count) {
    __asm__ volatile("rep outsw" : "+S"(addr), "+c"(word_count) : "d"(port));
}

// --- Функции ожидания ---
static void ata_delay() {
    port_byte_in(0x1F7); port_byte_in(0x1F7);
    port_byte_in(0x1F7); port_byte_in(0x1F7);
}

static int ata_wait(int check_drq) {
    int timeout = 100000;
    uint8_t status;
    while (--timeout) {
        status = port_byte_in(0x1F7);
        if (status == 0xFF) return -1; // Диск не подключен (Floating bus)
        if ((status & 0x80) == 0) break; // Ожидание сброса флага BSY
    }
    if (timeout == 0) return -1;

    if (check_drq) {
        timeout = 100000;
        while (--timeout) {
            status = port_byte_in(0x1F7);
            if (status & 0x08) break; // Ожидание установки флага DRQ
        }
        if (timeout == 0) return -1;
    }
    return 0;
}

// Проверка: подключен ли физический ATA диск на порту 0x1F0
int ata_is_ready() {
    uint8_t status = port_byte_in(0x1F7);
    if (status == 0xFF) return 0; // Диска нет

    // Пробуем выбрать Master диск
    port_byte_out(0x1F6, 0xE0);
    ata_delay();
    status = port_byte_in(0x1F7);

    if (status == 0xFF || status == 0x00) return 0; // Диск не отвечает
    return 1; // Физический диск доступен!
}

// --- Чтение и запись секторов ---
void read_sector(uint32_t lba, uint8_t* buffer) {
    port_byte_out(0x1F6, ((lba >> 24) & 0x0F) | 0xE0);
    port_byte_out(0x1F2, 1);
    port_byte_out(0x1F3, (uint8_t)lba);
    port_byte_out(0x1F4, (uint8_t)(lba >> 8));
    port_byte_out(0x1F5, (uint8_t)(lba >> 16));
    port_byte_out(0x1F7, 0x20); // Read Sectors

    ata_delay();
    if (ata_wait(1) == -1) return;

    insw(0x1F0, buffer, 256);
}

void write_sector(uint32_t lba, uint8_t* buffer) {
    port_byte_out(0x1F6, ((lba >> 24) & 0x0F) | 0xE0);
    port_byte_out(0x1F2, 1);
    port_byte_out(0x1F3, (uint8_t)lba);
    port_byte_out(0x1F4, (uint8_t)(lba >> 8));
    port_byte_out(0x1F5, (uint8_t)(lba >> 16));
    port_byte_out(0x1F7, 0x30); // Write Sectors

    ata_delay();
    if (ata_wait(1) == -1) return;

    outsw(0x1F0, buffer, 256);

    port_byte_out(0x1F7, 0xE7); // Flush Cache
    ata_delay();
    ata_wait(1);
}

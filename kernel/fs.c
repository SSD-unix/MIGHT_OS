/* kernel/fs.c */
#include "../common.h"
#include "../drivers/ata.h"
#include "utils.h"
#include "../drivers/screen.h"

#define FS_ROOT_SECTOR 30
#define ENTRY_SIZE 32 // Ровно 32 байта на каждый файл

// Подготовка диска
void format_disk() {
    uint8_t buffer[512];
    for(int i = 0; i < 512; i++) buffer[i] = 0;

    write_sector(FS_ROOT_SECTOR, buffer);
    kprint("File system initialized at sector 30.\n");
}

// Список файлов
void list_files() {
    uint8_t sector_buffer[512];
    read_sector(FS_ROOT_SECTOR, sector_buffer);

    int found = 0;
    for (int i = 0; i < 16; i++) {
        int offset = i * ENTRY_SIZE; // Смещение для каждого файла (0, 32, 64...)

        // 24-й байт - это наш флаг active
        if (sector_buffer[offset + 24] == 1) {
            kprint("- ");
            kprint((char*)&sector_buffer[offset]); // Имя всегда лежит в самом начале блока
            kprint("\n");
            found = 1;
        }
    }
    if (!found) kprint("No files found. Try FORMAT first.\n");
}

// Создание пустого файла
void touch_file(char* filename) {
    // === ДЕБАГ: Смотрим, что реально дошло до функции ===
    kprint("[DEBUG] touch_file received: '");
    kprint(filename);
    kprint("'\n");

    uint8_t sector_buffer[512];
    read_sector(FS_ROOT_SECTOR, sector_buffer);

    for (int i = 0; i < 16; i++) {
        int offset = i * ENTRY_SIZE;

        // Ищем пустой слот (24-й байт равен 0)
        if (sector_buffer[offset + 24] == 0) {

            // Копируем имя побайтово (максимум 15 символов)
            int j = 0;
            while(filename[j] != '\0' && j < 15) {
                sector_buffer[offset + j] = filename[j];
                j++;
            }
            sector_buffer[offset + j] = '\0';

            // Ставим флаг active (24-й байт)
            sector_buffer[offset + 24] = 1;

            // Записываем обновленный сектор на диск
            write_sector(FS_ROOT_SECTOR, sector_buffer);
            kprint("File successfully saved to disk.\n");
            return;
        }
    }
    kprint("Error: Directory full.\n");
}

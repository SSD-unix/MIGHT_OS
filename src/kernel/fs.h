#ifndef MIGHT_FS_H
#define MIGHT_FS_H

#include "../common.h"

#define MIGHT_FS_MAGIC       0x4D494748 // 'MIGH'
#define FS_SUPERBLOCK_LBA    30
#define FS_INODE_TABLE_LBA   31         // LBA 31..38
#define FS_DATA_START_LBA    39

#define TYPE_FILE 1
#define TYPE_DIR  2

#define MIGHT_EOF 0xFFFFFFFF            // Маркер конца файла

// В каждом секторе 4 байта под следующий LBA, 508 байт под данные
#define SECTOR_DATA_SIZE 508

struct __attribute__((packed)) MightSuperblock {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t first_data_lba;
    uint32_t total_inodes;
    uint32_t free_blocks;
};

// Inode ровно 32 байта
struct __attribute__((packed)) MightInode {
    char     name[16];          // Имя файла
    uint32_t size;              // Размер в байтах (любой!)
    uint32_t first_lba;         // Указатель на САМЫЙ ПЕРВЫЙ сектор
    uint8_t  type;              // 1 = FILE, 2 = DIR
    uint8_t  used;              // 1 = Занят, 0 = Свободен
    uint8_t  reserved[6];       // Выравнивание
};

void format_disk();
void list_files();
void touch_file(char* filename);
void make_directory(char* dirname);
void write_file(char* filename, char* data);
void read_file(char* filename, char* buffer);
void remove_file(char* filename);

#endif

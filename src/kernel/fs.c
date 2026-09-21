/* kernel/fs.c */
#include "../common.h"
#include "../drivers/ata.h"
#include "utils.h"
#include "../drivers/screen.h"

#define FS_ROOT_SECTOR 30
#define ENTRY_SIZE 32

#define MAX_RAM_FILES 16
#define RAM_FILE_SIZE 2048

struct RamFileSlot {
    char name[16];
    uint8_t active;
    char data[RAM_FILE_SIZE];
};

static struct RamFileSlot ram_disk[MAX_RAM_FILES];

// Вспомогательная очистка имени
static void trim_name(char* src, char* dest) {
    int i = 0;
    while (src[i] != '\0' && src[i] != ' ' && src[i] != '\t' &&
           src[i] != '\r' && src[i] != '\n' && i < 15) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// Сравнение строк
int fs_strcmp(char* str1, char* str2) {
    char clean1[16];
    char clean2[16];
    trim_name(str1, clean1);
    trim_name(str2, clean2);

    int i = 0;
    while (i < 15) {
        if (clean1[i] == '\0' && clean2[i] == '\0') return 0;
        if (clean1[i] != clean2[i]) return 1;
        i++;
    }
    return 0;
}

// --- Операции с RAMFS ---
static void ram_touch(char* filename) {
    char clean_name[16];
    trim_name(filename, clean_name);
    for (int i = 0; i < MAX_RAM_FILES; i++) {
        if (ram_disk[i].active == 0) {
            int j = 0;
            while (clean_name[j] != '\0' && j < 15) {
                ram_disk[i].name[j] = clean_name[j];
                j++;
            }
            ram_disk[i].name[j] = '\0';
            ram_disk[i].data[0] = '\0';
            ram_disk[i].active = 1;
            kprint("[RAMFS] File created in RAM.\n");
            return;
        }
    }
    kprint("[RAMFS] Error: Directory full.\n");
}

static void ram_write(char* filename, char* data) {
    char clean_name[16];
    trim_name(filename, clean_name);
    int idx = -1;

    for (int i = 0; i < MAX_RAM_FILES; i++) {
        if (ram_disk[i].active && fs_strcmp(clean_name, ram_disk[i].name) == 0) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        ram_touch(filename);
        for (int i = 0; i < MAX_RAM_FILES; i++) {
            if (ram_disk[i].active && fs_strcmp(clean_name, ram_disk[i].name) == 0) {
                idx = i;
                break;
            }
        }
    }

    if (idx != -1) {
        int j = 0;
        while (data[j] != '\0' && j < (RAM_FILE_SIZE - 1)) {
            ram_disk[idx].data[j] = data[j];
            j++;
        }
        ram_disk[idx].data[j] = '\0';
    }
}

static int ram_read(char* filename, char* buffer) {
    char clean_name[16];
    trim_name(filename, clean_name);
    for (int i = 0; i < MAX_RAM_FILES; i++) {
        if (ram_disk[i].active && fs_strcmp(clean_name, ram_disk[i].name) == 0) {
            int j = 0;
            while (ram_disk[i].data[j] != '\0' && j < (RAM_FILE_SIZE - 1)) {
                buffer[j] = ram_disk[i].data[j];
                j++;
            }
            buffer[j] = '\0';
            return 1;
        }
    }
    return 0;
}

// --- Публичный интерфейс (ГИБРИД) ---

void format_disk() {
    if (ata_is_ready()) {
        uint8_t buffer[512];
        for (int i = 0; i < 512; i++) buffer[i] = 0;
        write_sector(FS_ROOT_SECTOR, buffer);
        kprint("[HDD] Physical disk formatted at sector 30.\n");
    } else {
        for (int i = 0; i < MAX_RAM_FILES; i++) ram_disk[i].active = 0;
        kprint("[RAMFS] RAM disk formatted.\n");
    }
}

void list_files() {
    if (ata_is_ready()) {
        uint8_t sector_buffer[512];
        read_sector(FS_ROOT_SECTOR, sector_buffer);
        int found = 0;
        for (int i = 0; i < 16; i++) {
            int offset = i * ENTRY_SIZE;
            if (sector_buffer[offset + 24] == 1) {
                kprint("- ");
                char clean[16];
                trim_name((char*)&sector_buffer[offset], clean);
                kprint(clean);
                kprint("\n");
                found = 1;
            }
        }
        if (!found) kprint("[HDD] No files found. Try FORMAT first.\n");
    } else {
        int found = 0;
        for (int i = 0; i < MAX_RAM_FILES; i++) {
            if (ram_disk[i].active) {
                kprint("- ");
                kprint(ram_disk[i].name);
                kprint("\n");
                found = 1;
            }
        }
        if (!found) kprint("[RAMFS] No files in RAM.\n");
    }
}

void touch_file(char* filename) {
    if (ata_is_ready()) {
        uint8_t sector_buffer[512];
        read_sector(FS_ROOT_SECTOR, sector_buffer);
        char clean_filename[16];
        trim_name(filename, clean_filename);

        if (clean_filename[0] == '\0') return;

        for (int i = 0; i < 16; i++) {
            int offset = i * ENTRY_SIZE;
            if (sector_buffer[offset + 24] == 0) {
                int j = 0;
                while (clean_filename[j] != '\0' && j < 15) {
                    sector_buffer[offset + j] = clean_filename[j];
                    j++;
                }
                while (j < 16) { sector_buffer[offset + j] = '\0'; j++; }
                sector_buffer[offset + 24] = 1;
                write_sector(FS_ROOT_SECTOR, sector_buffer);
                kprint("[HDD] File created on physical disk.\n");
                return;
            }
        }
        kprint("[HDD] Directory full.\n");
    } else {
        ram_touch(filename);
    }
}

void write_file(char* filename, char* data) {
    if (ata_is_ready()) {
        uint8_t sector_buffer[512];
        read_sector(FS_ROOT_SECTOR, sector_buffer);
        int file_idx = -1;

        for (int i = 0; i < 16; i++) {
            int offset = i * ENTRY_SIZE;
            if (sector_buffer[offset + 24] == 1 && fs_strcmp(filename, (char*)&sector_buffer[offset]) == 0) {
                file_idx = i;
                break;
            }
        }

        if (file_idx == -1) {
            touch_file(filename);
            read_sector(FS_ROOT_SECTOR, sector_buffer);
            for (int i = 0; i < 16; i++) {
                int offset = i * ENTRY_SIZE;
                if (sector_buffer[offset + 24] == 1 && fs_strcmp(filename, (char*)&sector_buffer[offset]) == 0) {
                    file_idx = i;
                    break;
                }
            }
        }

        if (file_idx != -1) {
            int start_sector = FS_ROOT_SECTOR + 1 + (file_idx * 4);
            int data_idx = 0;
            for (int s = 0; s < 4; s++) {
                uint8_t data_sector[512];
                for (int j = 0; j < 512; j++) {
                    if (data[data_idx] != '\0') data_sector[j] = data[data_idx++];
                    else data_sector[j] = '\0';
                }
                write_sector(start_sector + s, data_sector);
            }
        }
    } else {
        ram_write(filename, data);
    }
}

void read_file(char* filename, char* buffer) {
    buffer[0] = '\0';

    if (ata_is_ready()) {
        uint8_t sector_buffer[512];
        read_sector(FS_ROOT_SECTOR, sector_buffer);

        for (int i = 0; i < 16; i++) {
            int offset = i * ENTRY_SIZE;
            if (sector_buffer[offset + 24] == 1) {
                if (fs_strcmp(filename, (char*)&sector_buffer[offset]) == 0) {
                    int start_sector = FS_ROOT_SECTOR + 1 + (i * 4);
                    int buf_idx = 0;
                    for (int s = 0; s < 4; s++) {
                        uint8_t data_sector[512];
                        read_sector(start_sector + s, data_sector);
                        for (int j = 0; j < 512; j++) {
                            if (data_sector[j] == '\0') {
                                buffer[buf_idx] = '\0';
                                return;
                            }
                            buffer[buf_idx++] = data_sector[j];
                        }
                    }
                    buffer[buf_idx] = '\0';
                    return;
                }
            }
        }
        kprint("[HDD] File not found.\n");
    } else {
        if (!ram_read(filename, buffer)) {
            kprint("[RAMFS] File not found.\n");
        }
    }
}

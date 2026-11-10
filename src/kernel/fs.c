#include "fs.h"
#include "../drivers/ata.h"
#include "../drivers/screen.h"
#include "utils.h"

extern void kprint(const char* str);
extern void kprint_int(int n);

static void memory_set(void* dest, uint8_t val, uint32_t len) {
    uint8_t* ptr = (uint8_t*)dest;
    for (uint32_t i = 0; i < len; i++) ptr[i] = val;
}

// Очистка от пробелов и переносов строк с гарантированным обнулением
static void trim_string(char* dest, const char* src, int max_len) {
    memory_set(dest, 0, max_len);
    int i = 0, j = 0;
    while (src[i] == ' ' || src[i] == '\t') i++;
    while (src[i] != '\0' && src[i] != ' ' && src[i] != '\n' && src[i] != '\r' && j < (max_len - 1)) {
        dest[j++] = src[i++];
    }
    dest[j] = '\0';
}

static int find_free_inode() {
    struct MightInode inodes[16];
    for (int s = 0; s < 8; s++) {
        read_sector(FS_INODE_TABLE_LBA + s, (uint8_t*)inodes);
        for (int i = 0; i < 16; i++) {
            if (!inodes[i].used) return (s * 16 + i);
        }
    }
    return -1;
}

static void write_inode(int inode_id, struct MightInode* in) {
    int s = inode_id / 16;
    int offset = inode_id % 16;
    struct MightInode inodes[16];

    read_sector(FS_INODE_TABLE_LBA + s, (uint8_t*)inodes);
    inodes[offset] = *in;
    write_sector(FS_INODE_TABLE_LBA + s, (uint8_t*)inodes);
}

// Проверка: используется ли LBA сектор в любой из цепочек файлов
static int is_lba_used(uint32_t check_lba) {
    struct MightInode inodes[16];
    for (int s = 0; s < 8; s++) {
        read_sector(FS_INODE_TABLE_LBA + s, (uint8_t*)inodes);
        for (int i = 0; i < 16; i++) {
            if (inodes[i].used && inodes[i].first_lba != 0) {
                uint32_t curr = inodes[i].first_lba;
                uint8_t buf[512];
                while (curr != MIGHT_EOF && curr != 0) {
                    if (curr == check_lba) return 1;
                    read_sector(curr, buf);
                    curr = *(uint32_t*)buf; // Первые 4 байта — следующий LBA
                }
            }
        }
    }
    return 0;
}

// Поиск первого свободного LBA сектора
static uint32_t alloc_free_lba() {
    for (uint32_t lba = FS_DATA_START_LBA; lba < 20000; lba++) {
        if (!is_lba_used(lba)) return lba;
    }
    return 0;
}

void format_disk() {
    if (!ata_is_ready()) {
        kprint("[MIGHT FS] Error: Physical disk not found!\n");
        return;
    }

    uint8_t sector_buf[512];
    memory_set(sector_buf, 0, 512);

    struct MightSuperblock* sb = (struct MightSuperblock*)sector_buf;
    sb->magic = MIGHT_FS_MAGIC;
    sb->total_blocks = 20000;
    sb->first_data_lba = FS_DATA_START_LBA;
    sb->total_inodes = 128;
    sb->free_blocks = 20000 - FS_DATA_START_LBA;
    write_sector(FS_SUPERBLOCK_LBA, sector_buf);

    memory_set(sector_buf, 0, 512);
    for (int i = 0; i < 8; i++) {
        write_sector(FS_INODE_TABLE_LBA + i, sector_buf);
    }

    struct MightInode root_inode;
    memory_set(&root_inode, 0, sizeof(struct MightInode));
    trim_string(root_inode.name, "/", 16);
    root_inode.size = 0;
    root_inode.first_lba = FS_DATA_START_LBA;
    root_inode.type = TYPE_DIR;
    root_inode.used = 1;

    // Записываем для корня пустой сектор с EOF
    memory_set(sector_buf, 0, 512);
    *(uint32_t*)sector_buf = MIGHT_EOF;
    write_sector(FS_DATA_START_LBA, sector_buf);

    write_inode(0, &root_inode);

    kprint("[MIGHT FS] Formatted with Root Directory.\n");
}

void list_files() {
    if (!ata_is_ready()) return;

    struct MightInode inodes[16];
    int found = 0;

    kprint("\n--- MIGHT FS Directory Listing ---\n");

    for (int s = 0; s < 8; s++) {
        read_sector(FS_INODE_TABLE_LBA + s, (uint8_t*)inodes);
        for (int i = 0; i < 16; i++) {
            if (inodes[i].used) {
                if (inodes[i].type == TYPE_DIR) {
                    kprint("[DIR]  ");
                } else {
                    kprint("[FILE] ");
                }

                kprint(inodes[i].name);

                int name_len = 0;
                while (inodes[i].name[name_len] != '\0' && name_len < 16) name_len++;
                for (int sp = 0; sp < (16 - name_len); sp++) kprint(" ");

                kprint(" | Size: ");
                kprint_int(inodes[i].size);
                kprint(" B | First LBA: ");
                kprint_int(inodes[i].first_lba);
                kprint("\n");

                found = 1;
            }
        }
    }
    if (!found) kprint("[MIGHT FS] No files or directories found.\n");
    kprint("----------------------------------\n\n");
}

void make_directory(char* dirname) {
    if (!ata_is_ready()) return;

    int inode_id = find_free_inode();
    if (inode_id == -1) {
        kprint("[MIGHT FS] Error: No free Inodes!\n");
        return;
    }

    uint32_t free_lba = alloc_free_lba();
    if (free_lba == 0) {
        kprint("[MIGHT FS] Error: Disk full!\n");
        return;
    }

    struct MightInode dir_inode;
    memory_set(&dir_inode, 0, sizeof(struct MightInode));
    trim_string(dir_inode.name, dirname, 16);
    dir_inode.size = 0;
    dir_inode.first_lba = free_lba;
    dir_inode.type = TYPE_DIR;
    dir_inode.used = 1;

    uint8_t empty_sector[512];
    memory_set(empty_sector, 0, 512);
    *(uint32_t*)empty_sector = MIGHT_EOF;
    write_sector(free_lba, empty_sector);

    write_inode(inode_id, &dir_inode);

    kprint("[MIGHT FS] Directory created: ");
    kprint(dir_inode.name);
    kprint("\n");
}

void touch_file(char* filename) {
    write_file(filename, "");
}

void write_file(char* filename, char* data) {
    if (!ata_is_ready()) return;

    char clean_fname[16];
    trim_string(clean_fname, filename, 16);

    if (clean_fname[0] == '\0') return;

    uint32_t data_len = 0;
    while (data[data_len] != '\0') data_len++;

    // 1. ИЩЕМ: Существует ли уже inode с таким именем?
    int target_inode = -1;
    struct MightInode inodes[16];

    for (int s = 0; s < 8; s++) {
        read_sector(FS_INODE_TABLE_LBA + s, (uint8_t*)inodes);
        for (int i = 0; i < 16; i++) {
            if (inodes[i].used && string_compare(inodes[i].name, clean_fname) == 0) {
                target_inode = s * 16 + i;
                break;
            }
        }
        if (target_inode != -1) break;
    }

    // 2. Если не нашли — берем свободный inode
    if (target_inode == -1) {
        target_inode = find_free_inode();
        if (target_inode == -1) {
            kprint("[MIGHT FS] Error: Inode table full!\n");
            return;
        }
    }

    // 3. Записываем файл цепочкой секторов
    uint32_t bytes_left = data_len;
    uint32_t data_offset = 0;
    uint32_t prev_lba = 0;
    uint32_t first_lba = 0;

    while (bytes_left > 0 || first_lba == 0) {
        uint32_t free_lba = alloc_free_lba();
        if (free_lba == 0) {
            kprint("[MIGHT FS] Error: Disk Full!\n");
            return;
        }

        if (first_lba == 0) first_lba = free_lba;

        // Если это не первый сектор — связываем предыдущий с текущим
        if (prev_lba != 0) {
            uint8_t prev_buf[512];
            read_sector(prev_lba, prev_buf);
            *(uint32_t*)prev_buf = free_lba;
            write_sector(prev_lba, prev_buf);
        }

        uint8_t sector_buf[512];
        memory_set(sector_buf, 0, 512);

        // Устанавливаем конец файла по умолчанию
        *(uint32_t*)sector_buf = MIGHT_EOF;

        // Заполняем оставшиеся 508 байт данными
        for (int i = 4; i < 512 && bytes_left > 0; i++) {
            sector_buf[i] = data[data_offset++];
            bytes_left--;
        }

        write_sector(free_lba, sector_buf);
        prev_lba = free_lba;

        if (bytes_left == 0) break;
    }

    // 4. Обновляем / Сохраняем Inode
    struct MightInode file_inode;
    memory_set(&file_inode, 0, sizeof(struct MightInode));
    trim_string(file_inode.name, clean_fname, 16);
    file_inode.size = data_len;
    file_inode.first_lba = first_lba;
    file_inode.type = TYPE_FILE;
    file_inode.used = 1;

    write_inode(target_inode, &file_inode);

    kprint("[MIGHT FS] File written: ");
    kprint(clean_fname);
    kprint("\n");
}

void read_file(char* filename, char* buffer) {
    if (!ata_is_ready()) return;

    buffer[0] = '\0';
    char clean_fname[16];
    trim_string(clean_fname, filename, 16);

    struct MightInode inodes[16];

    for (int s = 0; s < 8; s++) {
        read_sector(FS_INODE_TABLE_LBA + s, (uint8_t*)inodes);
        for (int i = 0; i < 16; i++) {
            if (inodes[i].used && inodes[i].type == TYPE_FILE) {
                if (string_compare(inodes[i].name, clean_fname) == 0) {
                    uint32_t curr_lba = inodes[i].first_lba;
                    uint32_t buf_idx = 0;

                    // Читаем по цепочке до MIGHT_EOF или выхода за size
                    while (curr_lba != MIGHT_EOF && curr_lba != 0 && buf_idx < inodes[i].size) {
                        uint8_t sector_buf[512];
                        read_sector(curr_lba, sector_buf);

                        uint32_t next_lba = *(uint32_t*)sector_buf;

                        for (int b = 4; b < 512 && buf_idx < inodes[i].size; b++) {
                            buffer[buf_idx++] = sector_buf[b];
                        }

                        curr_lba = next_lba;
                    }
                    buffer[buf_idx] = '\0';
                    return;
                }
            }
        }
    }
    kprint("[MIGHT FS] File not found.\n");
}

void remove_file(char* filename) {
    if (!ata_is_ready()) return;

    char clean_fname[16];
    trim_string(clean_fname, filename, 16);

    if (clean_fname[0] == '\0') return;

    struct MightInode inodes[16];

    for (int s = 0; s < 8; s++) {
        read_sector(FS_INODE_TABLE_LBA + s, (uint8_t*)inodes);
        for (int i = 0; i < 16; i++) {
            if (inodes[i].used && string_compare(inodes[i].name, clean_fname) == 0) {
                memory_set(&inodes[i], 0, sizeof(struct MightInode));
                write_sector(FS_INODE_TABLE_LBA + s, (uint8_t*)inodes);

                kprint("[MIGHT FS] File removed: ");
                kprint(clean_fname);
                kprint("\n");
                return;
            }
        }
    }
    kprint("[MIGHT FS] File not found.\n");
}

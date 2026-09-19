#!/usr/bin/env python3
import struct
import sys

# Настройки файловой системы MIGHT OS
FS_ROOT_SECTOR = 30
SECTOR_SIZE = 512
ENTRY_SIZE = 32

def create_image(bin_file, img_file):
    # 1. Создаем пустой диск размером 100000 секторов (~51 МБ)
    disk_size = 100000 * SECTOR_SIZE
    disk = bytearray(disk_size)

    # 2. Читаем скомпилированный hello.bin
    with open(bin_file, "rb") as f:
        app_bytes = f.read()

    filename = "HELLO".encode('ascii')

    # 3. Формируем запись о файле в корневом секторе (сектор 30)
    # [0..15]: Имя файла (16 байт)
    # [24]:    Флаг active = 1
    offset = FS_ROOT_SECTOR * SECTOR_SIZE
    disk[offset : offset + len(filename)] = filename
    disk[offset + 24] = 1  # active status

    # 4. Записываем тело программы в следующий сектор (сектор 31)
    app_sector_offset = (FS_ROOT_SECTOR + 1) * SECTOR_SIZE
    disk[app_sector_offset : app_sector_offset + len(app_bytes)] = app_bytes

    # 5. Сохраняем готовый data.img
    with open(img_file, "wb") as f:
        f.write(disk)

    print(f"[OK] {bin_file} записан в {img_file} (Сектор 30 -> метаданные, Сектор 31 -> код)")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Использование: python3 pack_disk.py <app.bin> <data.img>")
        sys.exit(1)
    create_image(sys.argv[1], sys.argv[2])

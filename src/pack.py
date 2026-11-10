#!/usr/bin/env python3
import struct
import sys
import os

if len(sys.argv) < 3:
    print("Usage: python3 pack.py <output_img> <input_file1> [input_file2 ...]")
    sys.exit(1)

output_img = sys.argv[1]
input_files = sys.argv[2:]

SECTOR_SIZE = 512
FS_SUPERBLOCK_LBA = 30
FS_INODE_TABLE_LBA = 31
FS_DATA_START_LBA = 39

MIGHT_FS_MAGIC = 0x4D494748  # 'MIGH'
MIGHT_EOF = 0xFFFFFFFF
TYPE_DIR = 0
TYPE_FILE = 1

# Увеличиваем размер образа до 32 МБ, чтобы 10-мегабайтный браузер гарантированно влез
img_size = 32 * 1024 * 1024
buffer = bytearray(img_size)

try:
    # 1. Запись Суперблока (LBA 30)
    total_blocks = img_size // SECTOR_SIZE
    sb_offset = FS_SUPERBLOCK_LBA * SECTOR_SIZE
    sb_bytes = struct.pack(
        "<IIIII",
        MIGHT_FS_MAGIC,
        total_blocks,
        FS_DATA_START_LBA,
        128,
        total_blocks - FS_DATA_START_LBA,
    )
    buffer[sb_offset : sb_offset + len(sb_bytes)] = sb_bytes

    # 2. Запись Root Inode #0 (для корректной совместимости с MIGHT FS)
    root_filename = b"/".ljust(16, b"\x00")
    root_inode_bytes = struct.pack(
        "<16sIIBB6s",
        root_filename,
        0,
        FS_DATA_START_LBA,
        TYPE_DIR,
        1,
        b"\x00" * 6,
    )
    root_inode_offset = FS_INODE_TABLE_LBA * SECTOR_SIZE
    buffer[root_inode_offset : root_inode_offset + len(root_inode_bytes)] = root_inode_bytes

    # Первым сектором данных отдаем пустой сектор для корня
    curr_lba = FS_DATA_START_LBA
    root_data_offset = curr_lba * SECTOR_SIZE
    buffer[root_data_offset : root_data_offset + 4] = struct.pack("<I", MIGHT_EOF)
    curr_lba += 1

    # 3. Упаковка файлов по очереди (начиная с Inode #1)
    for idx, input_file in enumerate(input_files):
        with open(input_file, "rb") as f:
            app_data = f.read()

        data_len = len(app_data)
        first_file_lba = curr_lba

        bytes_left = data_len
        data_offset = 0

        # Запись данных файла в цепочку секторов (508 байт данных + 4 байта next_lba)
        while bytes_left > 0 or data_len == 0:
            sector_offset = curr_lba * SECTOR_SIZE
            bytes_in_chunk = min(bytes_left, 508)

            next_lba = curr_lba + 1 if bytes_left > 508 else MIGHT_EOF
            buffer[sector_offset : sector_offset + 4] = struct.pack("<I", next_lba)

            if bytes_in_chunk > 0:
                buffer[
                    sector_offset + 4 : sector_offset + 4 + bytes_in_chunk
                ] = app_data[data_offset : data_offset + bytes_in_chunk]
                data_offset += bytes_in_chunk
                bytes_left -= bytes_in_chunk

            curr_lba += 1
            if next_lba == MIGHT_EOF:
                break

        # Запись Inode (начиная с index 1, так как index 0 занят корнем)
        raw_name = os.path.basename(input_file).split(".")[0].upper()
        filename = raw_name.encode("ascii").ljust(16, b"\x00")

        inode_bytes = struct.pack(
            "<16sIIBB6s",
            filename,
            data_len,
            first_file_lba,
            TYPE_FILE,
            1,
            b"\x00" * 6,
        )

        inode_slot = idx + 1
        inode_offset = (FS_INODE_TABLE_LBA * SECTOR_SIZE) + (inode_slot * 32)
        buffer[inode_offset : inode_offset + len(inode_bytes)] = inode_bytes

        print(
            f"[pack.py] Packed '{input_file}' as '{raw_name}' ({data_len} B) at Inode #{inode_slot}, LBA {first_file_lba}"
        )

    # 4. Сохранение итогового образа data.img
    with open(output_img, "wb") as f:
        f.write(buffer)

    print(f"[pack.py] Successfully created image '{output_img}' ({img_size // (1024*1024)} MB) with {len(input_files)} file(s).")

except Exception as e:
    print(f"Error packing image: {e}")
    sys.exit(1)

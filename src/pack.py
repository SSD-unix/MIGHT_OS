#!/usr/bin/env python3
import struct
import sys
import os

if len(sys.argv) < 3:
    print("Usage: python3 pack.py <input_bin> <output_img>")
    sys.exit(1)

input_file = sys.argv[1]
output_img = sys.argv[2]

SECTOR_SIZE = 512
FS_SUPERBLOCK_LBA = 30
FS_INODE_TABLE_LBA = 31
FS_DATA_START_LBA = 39

MIGHT_FS_MAGIC = 0x4D494748  # 'MIGH'
MIGHT_EOF = 0xFFFFFFFF
TYPE_FILE = 1

img_size = 10 * 1024 * 1024
buffer = bytearray(img_size)

try:
    with open(input_file, "rb") as f:
        app_data = f.read()

    data_len = len(app_data)

    # 1. Суперблок (LBA 30)
    sb_offset = FS_SUPERBLOCK_LBA * SECTOR_SIZE
    sb_bytes = struct.pack(
        "<IIIII",
        MIGHT_FS_MAGIC,
        20000,
        FS_DATA_START_LBA,
        128,
        20000 - FS_DATA_START_LBA,
    )
    buffer[sb_offset : sb_offset + len(sb_bytes)] = sb_bytes

    # 2. Запись данных файла цепочкой (LBA 39+)
    bytes_left = data_len
    data_offset = 0
    curr_lba = FS_DATA_START_LBA

    while bytes_left > 0 or data_len == 0:
        sector_offset = curr_lba * SECTOR_SIZE
        next_lba = curr_lba + 1 if bytes_left > 508 else MIGHT_EOF

        buffer[sector_offset : sector_offset + 4] = struct.pack("<I", next_lba)

        chunk_size = min(bytes_left, 508)
        if chunk_size > 0:
            buffer[sector_offset + 4 : sector_offset + 4 + chunk_size] = app_data[
                data_offset : data_offset + chunk_size
            ]
            data_offset += chunk_size
            bytes_left -= chunk_size

        if next_lba == MIGHT_EOF:
            break
        curr_lba = next_lba

    # 3. Inode #0 (Берем имя файла из аргумента, например BROWSER.BIN -> BROWSER)
    raw_name = os.path.basename(input_file).split('.')[0].upper()
    filename = raw_name.encode("ascii").ljust(16, b"\x00")

    inode_bytes = struct.pack(
        "<16sIIBB6s",
        filename,
        data_len,
        FS_DATA_START_LBA,
        TYPE_FILE,
        1,
        b"\x00" * 6,
    )

    inode_offset = FS_INODE_TABLE_LBA * SECTOR_SIZE
    buffer[inode_offset : inode_offset + len(inode_bytes)] = inode_bytes

    with open(output_img, "wb") as f:
        f.write(buffer)

    print(f"[pack.py] Packed '{input_file}' as '{raw_name}' ({data_len} B) into '{output_img}'")

except Exception as e:
    print(f"Error packing image: {e}")
    sys.exit(1)

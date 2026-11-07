#!/bin/bash
set -e

FLAGS="-O2 -nostdlib -ffreestanding -fno-stack-protector -fno-builtin -fno-rtti -fno-exceptions -fno-threadsafe-statics -I."

echo "[1/3] Compiling C++ sources..."
g++ $FLAGS -c might_cpp_runtime.cc -o might_cpp_runtime.o
g++ $FLAGS -c might_platform.cc -o might_platform.o
g++ $FLAGS -c main.cc -o main.o

echo "[2/3] Linking browser.bin..."
ld -m elf_x86_64 --oformat binary -e _start main.o might_platform.o might_cpp_runtime.o -o browser.bin

echo "[3/3] Packing into data.img..."
python3 ../pack.py browser.bin ../data.img

echo "Done! Run 'RUN BROWSER' in MIGHT OS."

# Пути для поиска исходников
VPATH = ../kernel:../kernel/apps:../drivers:../boot:..

# Включаем все файлы C, исключая старый VGA screen.c
RAW_C_SOURCES = $(wildcard ../kernel/*.c ../kernel/apps/*.c ../drivers/*.c ../*.c)
C_SOURCES = $(filter-out ../drivers/screen.c, $(RAW_C_SOURCES))

# В OBJ добавляем context_switch.o
OBJ = kernel_entry.o logo.o context_switch.o $(notdir $(C_SOURCES:.c=.o))

CC = gcc
LD = gcc

# Заменили -mno-sse на -msse2 для поддержки SIMD/SSE2 в flip_framebuffer
# Добавили -mno-red-zone для предотвращения порчи стека прерываниями в x86_64
CFLAGS = -g -ffreestanding -std=c99 -Wall -m64 -nostdlib -fno-stack-protector -fno-pie -msse2 -mno-red-zone
LDFLAGS = -m64 -nostdlib -no-pie -T ../linker.ld

IMAGE = might.iso
DATA_DISK = data.img

.PHONY: all run clean

all: $(IMAGE) $(DATA_DISK)

../hello.bin: ../hello.asm
	nasm -f bin $< -o $@

$(DATA_DISK): ../hello.bin
	python3 ../pack.py ../hello.bin $(DATA_DISK)

kernel.bin: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

kernel_entry.o: ../boot/kernel_entry.asm
	nasm -f elf64 $< -o $@

logo.o: ../boot/logo.asm ../boot/logo.raw
	nasm -I../boot/ -f elf64 $< -o $@

context_switch.o: ../kernel/context_switch.asm
	nasm -f elf64 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(IMAGE): kernel.bin
	@echo "=> Создаём ISO с графическим режимом..."
	mkdir -p isodir/boot/limine
	cp kernel.bin isodir/boot/
	cp /usr/share/limine/limine-bios.sys isodir/boot/limine/ 2>/dev/null || true
	cp /usr/share/limine/limine-bios-cd.bin isodir/boot/limine/ 2>/dev/null || true
	cp /usr/share/limine/limine-uefi-cd.bin isodir/boot/limine/ 2>/dev/null || true

	@echo 'timeout: 0' > limine.conf
	@echo '' >> limine.conf
	@echo '/MIGHT OS 1.1' >> limine.conf
	@echo '    protocol: multiboot2' >> limine.conf
	@echo '    kernel_path: boot():/boot/kernel.bin' >> limine.conf
	@echo '    resolution: 1366x768x32' >> limine.conf

	cp limine.conf isodir/limine.conf
	cp limine.conf isodir/boot/limine.conf
	cp limine.conf isodir/boot/limine/limine.conf
	rm -f limine.conf

	xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		isodir -o $(IMAGE)
	rm -rf isodir
	@echo "=> Внедряем MBR-загрузчик Limine..."
	limine bios-install $(IMAGE) 2>/dev/null || true
	@echo "=> Готово! Образ $(IMAGE) собран."

run: $(IMAGE) $(DATA_DISK)
	qemu-system-x86_64 \
		-drive file=$(DATA_DISK),format=raw,index=0,media=disk \
		-cdrom $(IMAGE) -m 256M -cpu qemu64 \
		-netdev user,id=net0 \
		-device e1000,netdev=net0 \
		-object filter-dump,id=f1,netdev=net0,file=net.pcap \
		-serial stdio

clean:
	rm -f *.bin *.o *.elf *.iso *.img ../hello.bin net.pcap
	rm -rf isodir limine.conf

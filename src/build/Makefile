# Пути для поиска исходников
VPATH = ../kernel:../drivers:..

# Собираем все .c файлы из ядра, драйверов и корня (common.c), игнорируя папку apps
# Благодаря этому e1000.c подхватится автоматически из папки drivers!
C_SOURCES = $(wildcard ../kernel/*.c ../drivers/*.c ../*.c)
# Конвертируем пути в имена объектных файлов для папки build
OBJ = kernel_entry.o $(notdir $(C_SOURCES:.c=.o))

CC = gcc
LD = gcc
CFLAGS = -g -ffreestanding -std=c99 -Wall -m64 -nostdlib -fno-stack-protector -fno-pie
LDFLAGS = -m64 -nostdlib -no-pie -T ../linker.ld

IMAGE = might.iso
DATA_DISK = data.img

all: $(IMAGE) $(DATA_DISK)

# 1. Компилируем приложение hello.asm в сырой бинарник (Flat Binary)
../hello.bin: ../hello.asm
	nasm -f bin $< -o $@

# 2. Упаковываем hello.bin в виртуальный диск data.img с помощью Python-скрипта
$(DATA_DISK): ../hello.bin
	python3 ../pack.py ../hello.bin $(DATA_DISK)

# 3. Линкуем ядро
kernel.bin: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

# 4. Компилируем точку входа (ассемблер)
kernel_entry.o: ../boot/kernel_entry.asm
	nasm -f elf64 $< -o $@

# 5. Компилируем все C-файлы в объектники
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 6. Собираем загрузочный ISO-образ с Limine
$(IMAGE): kernel.bin
	@echo "=> Создаём ISO..."
	mkdir -p isodir/boot/limine
	cp kernel.bin isodir/boot/
	cp /usr/share/limine/limine-bios.sys isodir/boot/limine/
	cp /usr/share/limine/limine-bios-cd.bin isodir/boot/limine/
	cp /usr/share/limine/limine-uefi-cd.bin isodir/boot/limine/
	@echo 'timeout: 3' > isodir/boot/limine/limine.conf
	@echo '/MIGHT OS 1.1' >> isodir/boot/limine/limine.conf
	@echo 'protocol: multiboot2' >> isodir/boot/limine/limine.conf
	@echo 'kernel_path: boot():/boot/kernel.bin' >> isodir/boot/limine/limine.conf
	xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		isodir -o $(IMAGE)
	limine bios-install $(IMAGE)
	rm -rf isodir
	@echo "=> Готово! Образ $(IMAGE) создан."


# Стало (добавляем drive с format=raw):
run: $(IMAGE) $(DATA_DISK)
	qemu-system-x86_64 \
		-drive file=$(DATA_DISK),format=raw,index=0,media=disk \
		-cdrom $(IMAGE) -m 256M -cpu qemu64 \
		-netdev user,id=net0 -device e1000,netdev=net0
clean:
	rm -f *.bin *.o *.elf *.iso *.img ../hello.bin
	rm -rf isodir

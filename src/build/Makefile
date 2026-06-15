# Список исходников
C_SOURCES = $(wildcard ../kernel/*.c ../drivers/*.c ../apps/*.c ../*.c)
# Превращаем путь/файл.c в файл.o
OBJ = kernel_entry.o $(notdir $(C_SOURCES:.c=.o))

CC = i386-elf-gcc
LD = i386-elf-ld
CFLAGS = -g -ffreestanding -std=c99 -Wall -m32
IMAGE = might.iso
DATA_DISK = data.img

# Указываем пути поиска для исходников, чтобы make видел их из разных папок
VPATH = ../kernel:../drivers:../apps:..

# =========================================================
# ГЛАВНАЯ ЦЕЛЬ: СБОРКА LIMINE ISO И ДИСКА ДАННЫХ
# =========================================================
all: $(IMAGE) $(DATA_DISK)

$(IMAGE): kernel.bin
	@echo "=> Создаем структуру ISO..."
	mkdir -p isodir/boot/limine
	cp kernel.bin isodir/boot/
	@echo "=> Копируем бинарники Limine..."
	cp /usr/share/limine/limine-bios.sys isodir/boot/limine/
	cp /usr/share/limine/limine-bios-cd.bin isodir/boot/limine/
	cp /usr/share/limine/limine-uefi-cd.bin isodir/boot/limine/
	@echo "=> Создаем limine.conf (Новый синтаксис)..."
	echo 'timeout: 3' > isodir/boot/limine/limine.conf
	echo '/MIGHT OS' >> isodir/boot/limine/limine.conf
	echo 'protocol: multiboot1' >> isodir/boot/limine/limine.conf
	echo 'kernel_path: boot():/boot/kernel.bin' >> isodir/boot/limine/limine.conf
	@echo "=> Собираем ISO образ..."
	xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		isodir -o $(IMAGE)
	@echo "=> Устанавливаем Limine MBR на ISO..."
	limine bios-install $(IMAGE)
	rm -rf isodir
	@echo "=> ГОТОВО! Образ $(IMAGE) собран."

# Создаём пустой диск данных для хранения файлов (50 МБ)
$(DATA_DISK):
	dd if=/dev/zero of=$(DATA_DISK) bs=512 count=100000 status=none

# =========================================================
# ЗАПУСК: диск данных на primary master (0x1F0), ISO на secondary (0x170)
# =========================================================
run: $(IMAGE) $(DATA_DISK)
	qemu-system-i386 -hda $(DATA_DISK) -cdrom $(IMAGE) -m 128M

kernel.bin: $(OBJ)
	$(LD) -m elf_i386 -T ../linker.ld -o $@ $^

kernel_entry.o: ../boot/kernel_entry.asm
	nasm $< -f elf32 -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.bin *.o *.elf *.iso *.img
	rm -rf isodir

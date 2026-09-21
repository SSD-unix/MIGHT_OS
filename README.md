<div align="center">

![MIGHT OS Logo](https://i.imgur.com/PgDOez3.jpeg)

# 🚀 MIGHT OS

[![License](https://img.shields.io/badge/license-Proprietary-red.svg)](#-license)
[![Architecture](https://img.shields.io/badge/architecture-x86__64-blue.svg)](#-tech-stack)
[![Bootloader](https://img.shields.io/badge/bootloader-Limine-brightgreen.svg)](#-tech-stack)
[![GitHub Stars](https://img.shields.io/github/stars/SSD-unix/MIGHT_OS?style=flat-square)](https://github.com/SSD-unix/MIGHT_OS/stargazers)
[![GitHub Issues](https://img.shields.io/github/issues/SSD-unix/MIGHT_OS?style=flat-square)](https://github.com/SSD-unix/MIGHT_OS/issues)

**A 64-bit Freestanding Operating System Built From Scratch**

[View on GitHub](https://github.com/SSD-unix/MIGHT_OS) • [Website](https://ssdunix.xyz) • [Developer SDK PDF](MIGHT_OS_Developer_Documentation.pdf)

</div>

---

## 📖 About the Project

**MIGHT OS** is a freestanding **64-bit (x86-64)** operating system built from the ground up in C and NASM Assembly. Booted via the modern **Limine Bootloader**, MIGHT OS serves as a powerful demonstration of low-level system engineering, hardware interaction, and kernel design.

From direct VGA memory manipulation to PCI scanning, custom flat-binary execution, and network framing — MIGHT OS implements every critical OS component without relying on standard C libraries or external kernel runtimes.

---

## ✨ Key Features

- ⚡ **64-Bit Long Mode Kernel**: Pure x86-64 execution built with GCC freestanding toolchain and Limine boot protocol.
- 🚀 **Userland Binary Loader**: Execute external flat 64-bit binaries loaded directly from disk sectors with memory safety (`.text` execution section).
- 🌐 **Intel e1000 Hardware Driver & Network Stack**: PCI Bus Scanner, DMA Ring Buffer transmission, and built-in **ICMP PING** tool.
- 💾 **Hybrid Storage & Filesystem**: Fast HDD-backed filesystem with automated sector-packing (`pack.py`) and memory fallback.
- 🖥️ **Interactive Shell & TTY**: Full keyboard scancode handler (Shift support, Backspace, Enter), VGA terminal controls, and system clocks.
- 📝 **Built-in `nano` Editor**: Integrated text editor with buffer scrolling and disk persistence.
- ⚙️ **BIOS-style `UX` Setup Utility**: Built-in graphical-like configuration menu rendered in VGA text mode.

---

## 🛠 Tech Stack

| Component | Technology |
|-----------|-----------|
| **Architecture** | x86-64 (AMD64 / Long Mode) |
| **Bootloader** | Limine Protocol (v8.x) |
| **Languages** | C (C99 Freestanding), NASM Assembly |
| **Networking** | Intel 82540EM (e1000) PCI Driver, DMA Ring Buffer |
| **Toolchain** | GCC, LD, NASM, Python 3 (Disk Packer) |
| **Emulation** | QEMU (`qemu-system-x86_64`) |

---

## ⚡ Interactive Shell Commands

MIGHT OS includes a full command-line shell:

```text
  HELP      - Display all available system commands
  LS        - List files in the filesystem
  RUN <app> - Read and execute 64-bit binary from disk sectors
  PING <host>- Transmit ICMP Echo Requests via Intel e1000 PCI card
  NETINIT   - Scan PCI bus and re-initialize e1000 hardware
  NANO <file>- Open built-in text editor
  CAT <file> - Display file contents to console
  TOUCH/ECHO- File management and text output
  UX        - Launch BIOS-style System Setup Utility
  INFO/TIME - Display system metrics and real-time clock
  CLEAR     - Reset terminal display

🚀 Building & Running
1. Prerequisites

Install required tools and build dependencies on Linux (Arch / Debian / Ubuntu):
Bash

# Ubuntu / Debian
sudo apt update
sudo apt install qemu-system-x86 gcc nasm make xorriso mtools python3 git

# Arch Linux
sudo pacman -S qemu-full gcc nasm make xorriso mtools python git

2. Install Limine Bootloader
Bash

git clone [https://github.com/limine-bootloader/limine.git](https://github.com/limine-bootloader/limine.git) --branch v8.x-branch-binary --depth=1
make -C limine
sudo make -C limine install

3. Build MIGHT OS & Pack Disk Image

Clone the repository and compile the kernel alongside user applications:
Bash

git clone [https://github.com/SSD-unix/MIGHT_OS.git](https://github.com/SSD-unix/MIGHT_OS.git)
cd MIGHT_OS/src/build

# Compile kernel, assemble hello.bin, and generate data.img + might.iso
make

4. Launch in QEMU

Run the OS with attached network card and raw disk drive:
Bash

make run

📄 Application SDK for Developers

MIGHT OS supports running custom 64-bit binaries written in Assembly or C.

    Write your binary using RIP-relative addressing:
    Code snippet

    [bits 64]
    global _start
    _start:
        push rbp
        mov rbp, rsp
        ; Your code here
        pop rbp
        ret

    Compile into a flat binary: nasm -f bin app.asm -o app.bin

    Pack into the disk image: python3 pack.py app.bin data.img

    Run inside MIGHT OS: > RUN HELLO

See the full MIGHT OS Developer Manual (PDF) for complete ABI and Syscall specifications.
📝 License
Plaintext

Copyright (c) 2026 SSD-unix. All rights reserved.

TERMS AND CONDITIONS FOR USE, DISTRIBUTION, AND REPRODUCTION

1. VIEWING RIGHTS ONLY:
   You are granted a non-exclusive right to view, read, and inspect the 
   source code of MIGHT OS solely for educational, evaluation, and review 
   purposes.

2. RESTRICTIONS:
   - You may NOT compile, run, execute, or deploy this source code or any 
     portion thereof without explicit written permission from the copyright holder.
   - You may NOT modify, fork, alter, or create derivative works based on 
     this source code.
   - You may NOT copy, paste, distribute, or incorporate any part of this 
     code into any other software or project.
   - You may NOT use this software or its source code for any commercial or 
     non-commercial purposes.

3. NO WARRANTY:
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.

📧 Contact & Support

    Author Website: ssdunix.xyz

    GitHub Issues: Report bugs or technical issues

⭐ If you find MIGHT OS interesting, please give the repository a star!

Back to Top

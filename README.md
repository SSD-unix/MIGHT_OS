<div align="center">

![MIGHT OS Logo](https://i.imgur.com/PgDOez3.jpeg)

# 🚀 MIGHT OS

[![License](https://img.shields.io/badge/license-Proprietary-red.svg)](#-license)
[![Architecture](https://img.shields.io/badge/architecture-x86__64-blue.svg)](#-tech-stack)
[![Bootloader](https://img.shields.io/badge/bootloader-Limine-brightgreen.svg)](#-tech-stack)
[![GitHub Stars](https://img.shields.io/github/stars/SSD-unix/MIGHT_OS?style=flat-square)](https://github.com/SSD-unix/MIGHT_OS/stargazers)
[![GitHub Issues](https://img.shields.io/github/issues/SSD-unix/MIGHT_OS?style=flat-square)](https://github.com/SSD-unix/MIGHT_OS/issues)

**A 64-bit freestanding operating system built from scratch**

[View on GitHub](https://github.com/SSD-unix/MIGHT_OS) • [Website](https://os.ssdunix.xyz) • [DOWNLOAD](https://archive.org/download/might-1.1-alpha-too-many-bugs-545/might.iso) • [Developer SDK PDF](https://ia903205.us.archive.org/6/items/might-1.1-alpha-too-many-bugs-545/MIGHT_OS_Developer_Documentation.pdf)
</div>

---

## 📖 About the Project

**MIGHT OS** is a freestanding **64-bit (x86-64)** operating system built from the ground up in C and NASM assembly. It is booted through the modern **Limine bootloader** and is intended as a hands-on learning project for understanding how operating systems work internally.

From direct VGA memory manipulation to PCI scanning, flat-binary execution, and low-level networking, MIGHT OS implements many core OS features without relying on standard libc or a full desktop environment. It is designed to be educational, minimal, and deeply technical.

---

## ✨ Key Features

- ⚡ **64-bit long mode kernel** built with a freestanding GCC toolchain
- 🚀 **Userland binary loader** for executing flat 64-bit binaries from disk
- 🌐 **Intel e1000 driver and network stack** with DMA ring buffers and ICMP support
- 💾 **Hybrid filesystem and storage layer** with sector packing logic
- 🖥️ **Interactive shell and TTY** with keyboard handling, text control, and clock support
- 📝 **Built-in nano-style editor** with persistent buffer editing
- ⚙️ **BIOS-like UX setup utility** rendered in VGA text mode
- 🔧 **Low-level hardware access** including PCI, VGA, and runtime system services

---

## 🛠️ Tech Stack

| Component | Technology |
|-----------|-----------|
| **Architecture** | x86-64 (AMD64 / Long Mode) |
| **Bootloader** | Limine Protocol (v8.x) |
| **Languages** | C (C99 freestanding), NASM assembly |
| **Networking** | Intel 82540EM (e1000) PCI driver, DMA ring buffer |
| **Toolchain** | GCC, LD, NASM, Python 3 |
| **Emulation** | QEMU (`qemu-system-x86_64`) |

---

## ⚙️ Interactive Shell Commands

MIGHT OS includes a full command-line shell with the following built-in commands:

```text
HELP      - Display available system commands
LS        - List files in the filesystem
RUN <app> - Load and execute a 64-bit binary from disk sectors
PING <host> - Send ICMP Echo Requests through the e1000 NIC
NETINIT   - Scan the PCI bus and initialize the network adapter
NANO <file> - Open the built-in text editor
CAT <file> - Display file contents in the terminal
TOUCH/ECHO - Create files and print text output
UX        - Launch the BIOS-style system setup utility
INFO/TIME - Show system metrics and the real-time clock
CLEAR     - Reset the terminal display
```

---

## 🚀 Getting Started

### 1. Prerequisites

Install the required dependencies on Ubuntu/Debian:

```bash
sudo apt update
sudo apt install qemu-system-x86 gcc nasm make xorriso mtools python3 git
```

On Arch Linux:

```bash
sudo pacman -S qemu-full gcc nasm make xorriso mtools python git
```

### 2. Install Limine

```bash
git clone https://github.com/limine-bootloader/limine.git --branch v8.x-branch-binary --depth=1
make -C limine
sudo make -C limine install
```

### 3. Build MIGHT OS

```bash
git clone https://github.com/SSD-unix/MIGHT_OS.git
cd MIGHT_OS/src/build
make
```

This compiles the kernel, assembles runtime components, and generates the disk image and ISO output.

### 4. Run in QEMU

```bash
make run
```

The project is configured to run with the network card and raw disk image attached for testing and development.

---

## 📄 Developer SDK

MIGHT OS supports running custom 64-bit user binaries written in assembly or C.

Example assembly stub:

```asm
[bits 64]
global _start

_start:
    push rbp
    mov rbp, rsp
    ; Your code here
    pop rbp
    ret
```

Compile and pack the binary:

```bash
nasm -f bin app.asm -o app.bin
python3 pack.py app.bin data.img
```

Then run it inside the OS:

```text
> RUN HELLO
```

See the full developer documentation PDF for complete ABI and syscall details.

---

## 🧬 Credits & Acknowledgments

This project is based on the work of Denis Nikulin's original OS project and has been heavily modified and expanded into the MIGHT OS codebase.

---

## 📝 License

Copyright (c) 2026 SSD-unix. All rights reserved.

TERMS AND CONDITIONS FOR USE, DISTRIBUTION, AND REPRODUCTION

1. VIEWING RIGHTS ONLY:
   You are granted a non-exclusive right to view, read, and inspect the source code of MIGHT OS solely for educational, evaluation, and review purposes.

2. RESTRICTIONS:
   - You may NOT compile, run, execute, or deploy this source code or any portion thereof without explicit written permission from the copyright holder.
   - You may NOT modify, fork, alter, or create derivative works based on this source code.
   - You may NOT copy, paste, distribute, or incorporate any part of this code into any other software or project.
   - You may NOT use this software or its source code for any commercial or non-commercial purposes.

3. NO WARRANTY:
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.

---

## 📧 Contact & Support

- Website: [ssdunix.xyz](https://ssdunix.xyz)
- GitHub Issues: [Report bugs or technical issues](https://github.com/SSD-unix/MIGHT_OS/issues)

---

<div align="center">

⭐ If you find MIGHT OS interesting, please consider giving the repository a star!

[Back to Top](#-might-os)

</div>

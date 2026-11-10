![MIGHT OS logo](https://i.imgur.com/PgDOez3.jpeg)

# 🚀 MIGHT OS

<div align="center">

[![License](https://img.shields.io/badge/license-Custom%20Proprietary-red.svg)](LICENSE)
[![GitHub Stars](https://img.shields.io/github/stars/SSD-unix/MIGHT_OS?style=flat-square)](https://github.com/SSD-unix/MIGHT_OS/stargazers)
[![GitHub Issues](https://img.shields.io/github/issues/SSD-unix/MIGHT_OS?style=flat-square)](https://github.com/SSD-unix/MIGHT_OS/issues)

**An experimental educational operating system built from scratch**

[![DOWNLOAD ISO](https://img.shields.io/badge/DOWNLOAD-ISO-blue?style=for-the-badge)](https://github.com/SSD-unix/MIGHT_OS/raw/refs/heads/master/src/build/might.iso)

[Source](https://github.com/SSD-unix/MIGHT_OS) · [Report a bug](https://github.com/SSD-unix/MIGHT_OS/issues) · [Request a feature](https://github.com/SSD-unix/MIGHT_OS/issues)

</div>

---

## About

MIGHT OS is a small operating-system project for exploring kernel development, boot processes, and low-level programming. The source tree includes boot code, a C kernel, drivers, networking, and a browser-inspired application environment.

The project is primarily written in C, with C++ and x86-64 assembly. The build creates a Limine-bootable ISO and a data disk image, and provides a QEMU run target.

## Features and project areas

- Custom kernel and x86-64 boot code
- Limine-based bootable ISO with BIOS and UEFI boot files
- Graphics and device-driver code
- Networking code, including lwIP integration and e1000 support
- Browser and user applications under `src/browser` and `src/kernel/apps`
- A small image-packing tool for the project's data disk format

Features are under active development; expect incomplete or changing behavior.

## Repository layout

| Path | Contents |
| --- | --- |
| `src/boot` | Boot and startup assembly |
| `src/kernel` | Kernel and application code |
| `src/drivers` | Device drivers |
| `src/net` | Networking code |
| `src/browser` | Browser component |
| `src/lib` | Shared libraries |
| `src/build` | Makefile and generated build outputs |
| `src/pack.py` | Packs an application binary into a MIGHT data image |

## Build and run on Arch Linux

The instructions below use Arch Linux packages and the toolchain defined by `src/build/Makefile`; a separate cross-compiler setup is not required by this build.

### 1. Install dependencies

Update the package database and install the compiler, assembler, build tools, Limine files, ISO utility, and QEMU x86 emulator:

```bash
sudo pacman -Syu
sudo pacman -S --needed base-devel git nasm python xorriso limine qemu-system-x86
```

The Makefile expects Limine's BIOS/UEFI files under `/usr/share/limine/`. If the build reports that one of those files is missing, verify that the installed Limine package provides the expected files.

### 2. Clone the repository

```bash
git clone https://github.com/SSD-unix/MIGHT_OS.git
cd MIGHT_OS
```

### 3. Build the ISO and data disk

Run `make` from the build directory:

```bash
cd src/build
make
```

The build outputs `might.iso` and `data.img` in `src/build/`. The Makefile assembles the boot and application code, compiles and links the kernel, then packages the ISO and data image.

### 4. Run in QEMU

From `src/build`, launch the project with:

```bash
make run
```

The run target starts `qemu-system-x86_64` with the generated ISO and data disk. To remove generated files and rebuild from scratch, run `make clean` in the same directory.

> **Safety:** Run MIGHT OS in an emulator while experimenting. It is experimental software; do not boot it on a machine containing important data or use it as your primary operating system.

## Contributing

Contributions and bug reports are welcome. Open an issue to discuss a change, then submit a pull request with a focused description and testing notes.

## License

This project is distributed under a custom proprietary license. See [LICENSE](LICENSE) for the full terms. Viewing is permitted for evaluation and educational review only; redistribution, modification, execution, and other uses require explicit written permission from the copyright holder.

## Support

- [Report bugs or request features](https://github.com/SSD-unix/MIGHT_OS/issues)

---

<div align="center">

[Back to top](#-might-os)

</div>

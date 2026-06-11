# RitOS - Modern C++ Operating System

RitOS is a custom x86 32-bit freestanding operating system designed with a custom C++ GUI Desktop environment running in VGA Text Mode (80x25 characters, 16 colors). It features a Virtual File System, File Explorer with CRUD actions, a Text Editor, a Calculator, Clock, Calendar, System Monitor, and custom block-art icons.

---

## 📋 Prerequisites

To build and run RitOS, you need an x86/x86_64 Linux host system (such as Ubuntu or Debian) with the following packages installed:

1. **Build Tools**: `gcc` and `g++` with 32-bit multilib support (since RitOS runs in 32-bit Protected Mode).
2. **ISO Creation Utilities**: `grub-mkrescue` and `xorriso` (used to package the OS binary into a bootable ISO).
3. **Emulator**: `qemu-system-x86` (to run the compiled ISO).

### Installing Dependencies (Ubuntu/Debian)

Run the following command to install all necessary packages:
```bash
sudo apt update && sudo apt install -y \
    build-essential \
    gcc-multilib \
    g++-multilib \
    grub-pc-bin \
    xorriso \
    mtools \
    qemu-system-x86
```

---

## 🛠️ How to Build the Project

RitOS can be built using either the provided shell script or GNU Make.

### Method 1: Using the Build Script (Recommended)

The build script automatically cleans old binaries, sets up any paths for tools, and compiles the bootable ISO:
```bash
./scripts/build_iso.sh
```

### Method 2: Using GNU Make Directly

If you prefer to run the compilation steps manually:
```bash
# Clean previous build artifacts
make clean

# Compile the kernel, userspace, and applications, and generate ritos.iso
make
```

Upon successful compilation, two main targets will be generated under `build/`:
*   `build/ritos.bin`: The raw kernel ELF binary.
*   `build/ritos.iso`: The bootable ISO image containing the GRUB bootloader configuration and the kernel binary.

---

## 📁 Source Layout

The tree separates portable code from machine-specific code, then organizes
the portable side by privilege layer. RitOS targets many machines (x86 today;
x64, ARM, embedded/IoT devices planned), so everything CPU- or board-specific
lives in a port under `architecture/` and the rest of the tree never changes between
targets.

```
architecture/   One subdirectory per CPU/platform port (see architecture/README.md)
  x86/            the i386 BIOS/GRUB port (the only one implemented today)
    architecture.mk  toolchain, target flags, image packaging for this port
    boot/            multiboot entry (boot.s), GRUB config, linker script
    drivers/         PS/2 keyboard & mouse, VGA terminal, power, ATA disk
    include/         architecture-provided <kernel/...> headers (input_output.h)
kernel/         The portable kernel
  core/           kernel entry, memory management
  drivers/        hardware-independent drivers (framebuffer renderer,
                  FAT filesystem driver over the storage interface)
  library/        freestanding helpers (string)
  include/        public kernel headers - the interfaces each port implements
userspace/      Everything that runs above the kernel (fully portable)
  interface/      the kernel<->user API surface (<ritos/interface.hpp>)
  runtime/        app startup glue: application_shim, runtime_support,
                  system_startup
  libraries/
    rit/            core userland library: string, object, virtual
                    filesystem, system (<rit/...>)
    graphical_interface/  window/desktop toolkit (<ritos/...>)
    compositor/     experimental compositor stack (not yet built)
  shell/          components that ARE the desktop: desktop, taskbar,
                  start_menu, status_bars
  applications/   ordinary launchable apps: calculator, terminal,
                  text_editor, file_manager, system_monitor, ...
assets/         Graphics and fonts
  sources/        real sources (svg)
  generated/      headers and icon blobs generated from them
tools/          Host-side build tools: embed_filesystem, elf_to_rbx,
                generators
scripts/        Build orchestration (build_iso.sh)
documentation/  Project documentation
build/          All build output (gitignored)
```

Builds select a port with `make ARCH=x86` (the default).

---

## 📦 RBX — the native executable format

`.rbx` is RitOS's own executable format — the equivalent of Windows' `.exe`.
Every program the OS runs (the desktop shell itself included) is an `.rbx`
file: a self-describing 64-byte header (magic `RBX2`, format version, target
CPU architecture, entry type, load address, entry point, code/bss sizes, a
checksum, and the program name) followed by the raw program image. The format
is defined once in `userspace/libraries/rit/include/rit/rbx_format.h`, shared by the
in-OS loader and the build tools, and the loader validates every field before
executing anything — corrupt, truncated, or wrong-architecture binaries are
refused.

`.rbx` files can be run from anywhere in the OS:

*   **File Explorer** — select an `.rbx` file and press *Open* (or Enter).
*   **Terminal** — `run <name>` or `run /system/executables/<name>.rbx`.
*   **Desktop / Start Menu** — icons launch the corresponding `.rbx`.

At build time, `tools/elf_to_rbx.cpp` packages the linker's intermediate ELF
output into RBX; the OS itself never sees or understands ELF.

Each `.rbx` also carries its own 32×32 icon (embedded by the build from
`assets/sources/icons/*.svg`), so launchers read an app's icon from the
executable itself — nothing is hardcoded in the desktop.

### Shortcuts (`.stct`)

Desktop icons and start-menu entries are **shortcut files**, not hardcoded
lists. A `.stct` file's content is simply the path of the target executable
(e.g. `/system/executables/clock.rbx`). The desktop shows every shortcut in
the current user's `/users/ramim/desktop` folder; the start menu shows
everything in `/users/ramim/launcher`. The displayed name is
the shortcut's filename with the extension hidden, and the icon is read from
the target `.rbx`. In the File Explorer, shortcuts appear with their full
`.stct` name, and opening one launches its target.

---

## 🗂️ The OS filesystem

RitOS organizes its own filesystem by who owns the data and when it changes:

```
/system/         the OS itself: shell components and executables (.rbx),
                 manifest.txt
/users/ramim/    the user's world: desktop/ and launcher/ shortcuts,
                 documents/, configuration/
/volumes/        mounted real storage (see below)
/temporary/      scratch space
```

---

## 💽 Real storage support

RitOS detects and operates real disks. The x86 port ships an ATA PIO driver;
on top of it a portable filesystem layer identifies what is on the disk
(FAT12/16/32, ext2/3/4, NTFS, exFAT - whole-disk or MBR-partitioned) and can
read and write files on FAT16/FAT32 volumes. In the Terminal:

*   `disk` — show the detected drive, capacity, and filesystem.
*   `mount` — import the volume's files into `/volumes/disk`.
*   `save <file>` — write a file from RitOS out to the physical disk.

Attach a disk in QEMU with:
```bash
qemu-system-i386 -cdrom build/ritos.iso -boot d \
    -drive file=disk.img,format=raw,if=ide
```

---

## 🚀 How to Run the Project

You can run the bootable ISO using the QEMU emulator:

```bash
qemu-system-i386 -cdrom build/ritos.iso
```

### Emulation Controls & Keyboard Shortcuts

Once the system boots, you will see a GRUB bootloader menu. Press **Enter** to boot RitOS. After the boot sequence loads, the C++ GUI desktop will launch:

*   **Mouse Interaction**: Left-click on desktop shortcut icons to launch applications. Drag window header bars to reposition them. Use button controls such as `[X]` to close windows or `[-]` to minimize them to the taskbar.
*   **Active Focus**: Tab key (`[Tab]`) cycles focus between active buttons or fields on the desktop.
*   **Closing Windows**: Press the Escape key (`[Esc]`) to close the currently focused active window.
*   **Start Menu**: Click `☼ Start` in the bottom-left corner of the taskbar to toggle the popup Start Menu. Click `Shut Down` inside the menu to exit.

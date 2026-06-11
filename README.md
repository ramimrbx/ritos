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

# Compile the kernel, SDK, and GUI, and generate ritos.iso
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
lives in a port under `arch/` and the rest of the tree never changes between
targets.

```
arch/        One subdirectory per CPU/platform port (see arch/README.md)
  x86/         the i386 BIOS/GRUB port (the only one implemented today)
    arch.mk      toolchain, target flags, image packaging for this port
    boot/        multiboot entry (boot.s), GRUB config, linker script
    drivers/     PS/2 keyboard & mouse, VGA text terminal, port-I/O power
    include/     arch-provided <kernel/...> headers (io.h)
kernel/      The portable kernel
  core/        kernel entry, memory management
  drivers/     hardware-independent drivers (linear framebuffer renderer)
  lib/         freestanding helpers (string)
  include/     public kernel headers — the interfaces each port implements
user/        Everything that runs above the kernel (fully portable)
  api/         the kernel<->user API surface (<ritos/api.hpp>)
  runtime/     app startup glue: app_shim, cpp_support, sdk_entry
  lib/
    rit/         core userland library: string, object, vfs, system (<rit/...>)
    gui/         window/desktop toolkit (<ritos/...>)
    modern_gui/  experimental compositor stack (not yet built)
  shell/       components that ARE the desktop: desktop, taskbar, startmenu, statusbars
  apps/        ordinary launchable apps: calculator, terminal, texteditor, ...
assets/      Graphics and fonts
  src/         real sources (svg)
  gen/         headers generated from them (do not hand-edit)
tools/       Host-side build tools: bin2c, elf2rbx, asset generators
scripts/     Build orchestration (build_iso.sh)
build/       All build output (gitignored)
```

Builds select a port with `make ARCH=x86` (the default).

---

## 📦 RBX — the native executable format

`.rbx` is RitOS's own executable format — the equivalent of Windows' `.exe`.
Every program the OS runs (the desktop shell itself included) is an `.rbx`
file: a self-describing 64-byte header (magic `RBX2`, format version, target
CPU architecture, entry type, load address, entry point, code/bss sizes, a
checksum, and the program name) followed by the raw program image. The format
is defined once in `user/lib/rit/include/rit/rbx_format.h`, shared by the
in-OS loader and the build tools, and the loader validates every field before
executing anything — corrupt, truncated, or wrong-architecture binaries are
refused.

`.rbx` files can be run from anywhere in the OS:

*   **File Explorer** — select an `.rbx` file and press *Open* (or Enter).
*   **Terminal** — `run <name>` or `run /sys/<name>.rbx`.
*   **Desktop / Start Menu** — icons launch the corresponding `.rbx`.

At build time, `tools/elf2rbx.cpp` packages the linker's intermediate ELF
output into RBX; the OS itself never sees or understands ELF.

Each `.rbx` also carries its own 32×32 icon (embedded by the build from
`assets/src/icons/*.svg`), so launchers read an app's icon from the
executable itself — nothing is hardcoded in the desktop.

### Shortcuts (`.stct`)

Desktop icons and start-menu entries are **shortcut files**, not hardcoded
lists. A `.stct` file's content is simply the path of the target executable
(e.g. `/sys/clock.rbx`). The desktop shows every shortcut in the `/desktop`
folder; the start menu shows everything in `/launcher`. The displayed name is
the shortcut's filename with the extension hidden, and the icon is read from
the target `.rbx`. In the File Explorer, shortcuts appear with their full
`.stct` name, and opening one launches its target.

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

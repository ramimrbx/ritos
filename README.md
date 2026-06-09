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

Upon successful compilation, two main targets will be generated in the project root directory:
*   `ritos.bin`: The raw kernel ELF binary.
*   `ritos.iso`: The bootable ISO image containing the GRUB bootloader configuration and the kernel binary.

---

## 🚀 How to Run the Project

You can run the bootable ISO using the QEMU emulator:

```bash
qemu-system-i386 -cdrom ritos.iso
```

### Emulation Controls & Keyboard Shortcuts

Once the system boots, you will see a GRUB bootloader menu. Press **Enter** to boot RitOS. After the boot sequence loads, the C++ GUI desktop will launch:

*   **Mouse Interaction**: Left-click on desktop shortcut icons to launch applications. Drag window header bars to reposition them. Use button controls such as `[X]` to close windows or `[-]` to minimize them to the taskbar.
*   **Active Focus**: Tab key (`[Tab]`) cycles focus between active buttons or fields on the desktop.
*   **Closing Windows**: Press the Escape key (`[Esc]`) to close the currently focused active window.
*   **Start Menu**: Click `☼ Start` in the bottom-left corner of the taskbar to toggle the popup Start Menu. Click `Shut Down` inside the menu to exit.

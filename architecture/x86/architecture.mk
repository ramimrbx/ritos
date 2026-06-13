# ── x86 port (i386 protected mode, BIOS + GRUB multiboot) ─────────────────
# Every port supplies an architecture.mk like this one. It must define:
#   CC/CXX/AS/LD     - the toolchain for the target
#   TARGET_CFLAGS    - code-generation flags for C/C++
#   TARGET_ASFLAGS   - flags for assembling boot/startup code
#   TARGET_LDEMU     - ld emulation (-m) for the target
#   LINKER_SCRIPT    - kernel linker script
#   ARCH_C_SOURCES   - architecture-specific drivers compiled into the kernel
#   ARCH_OBJS        - extra objects (boot/startup code)
#   IMAGE            - the bootable image target, with its packaging recipe
#   QEMU             - emulator command used by `make run`

CC  = gcc
CXX = g++
AS  = gcc
LD  = ld

TARGET_CFLAGS  = -m32
TARGET_ASFLAGS = -m32 -c
TARGET_LDEMU   = elf_i386

ARCH_DIR       = architecture/x86
LINKER_SCRIPT  = $(ARCH_DIR)/boot/linker.ld
ARCH_C_SOURCES = $(wildcard $(ARCH_DIR)/drivers/*.c)
ARCH_OBJS      = $(BUILD)/$(ARCH_DIR)/boot/boot.o

QEMU = qemu-system-i386 -cdrom

# Bootable image for this port: hybrid El Torito ISO via GRUB. With no -d,
# grub-mkrescue bundles every installed platform (i386-pc + x86_64-efi
# here), so the same ISO boots from legacy BIOS and from UEFI firmware.
VERSION ?= $(shell grep -o 'Version [0-9.]*' userspace/libraries/graphical_interface/source/application_windows.cpp | head -n 1 | cut -d' ' -f2)
IMAGE = $(BUILD)/ritos-v$(VERSION).iso

$(IMAGE): $(BIN) $(ARCH_DIR)/boot/grub.cfg
	mkdir -p $(BUILD)/iso_root/boot/grub
	cp $(BIN) $(BUILD)/iso_root/boot/
	cp $(ARCH_DIR)/boot/grub.cfg $(BUILD)/iso_root/boot/grub/
	grub-mkrescue -o $(IMAGE) $(BUILD)/iso_root
	rm -rf $(BUILD)/iso_root

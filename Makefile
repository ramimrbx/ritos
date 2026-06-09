CC = gcc
CXX = g++
AS = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic -fno-stack-protector -Ikernel/include -nostdlib
CXXFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-pie -fno-pic -fno-stack-protector -Ikernel/include -Isdk/framework/include -Isdk/api/include -Isdk/gui/include -nostdlib
ASFLAGS = -m32 -c
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib -no-pie
LDFLAGS_APP = -m elf_i386 -nostdlib

# App Addresses
desktop_ADDR = 0x2000040
taskbar_ADDR = 0x2100040
startmenu_ADDR = 0x2200040
statusbars_ADDR = 0x2300040
calculator_ADDR = 0x2400040
texteditor_ADDR = 0x2500040
filemanager_ADDR = 0x2600040
calendar_ADDR = 0x2700040
settings_ADDR = 0x2800040
clock_ADDR = 0x2900040
sysmon_ADDR = 0x2A00040
terminal_ADDR = 0x2B00040

# App Targets
APPS_RBX = sdk/apps/desktop.rbx \
           sdk/apps/taskbar.rbx \
           sdk/apps/startmenu.rbx \
           sdk/apps/statusbars.rbx \
           sdk/apps/calculator.rbx \
           sdk/apps/texteditor.rbx \
           sdk/apps/filemanager.rbx \
           sdk/apps/calendar.rbx \
           sdk/apps/settings.rbx \
           sdk/apps/clock.rbx \
           sdk/apps/sysmon.rbx \
           sdk/apps/terminal.rbx

# Kernel files
C_SOURCES = $(wildcard kernel/src/*.c)
KERNEL_CPP_SOURCES = sdk/framework/src/cpp_support.cpp \
                     sdk/framework/src/system.cpp \
                     sdk/framework/src/vfs.cpp \
                     sdk/framework/src/embedded_apps.cpp \
                     sdk/api/src/api.cpp \
                     sdk/api/src/sdk_entry.cpp

OBJ = boot/boot.o \
      $(C_SOURCES:.c=.o) \
      $(KERNEL_CPP_SOURCES:.cpp=.o)

BIN = ritos.bin
ISO = ritos.iso

all: $(BIN) $(ISO)

$(BIN): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

boot/boot.o: boot/boot.s
	$(AS) $(ASFLAGS) $< -o $@

scripts/bin2c: scripts/bin2c.cpp
	g++ -O2 $< -o $@

scripts/elf2rbx: scripts/elf2rbx.cpp
	g++ -O2 $< -o $@

# VFS Embedding Rule
sdk/framework/src/embedded_apps.cpp: $(APPS_RBX) scripts/bin2c
	./scripts/bin2c sdk/apps sdk/framework/src/embedded_apps.cpp

# RBX packaging rules
%.rbx: %.elf scripts/elf2rbx
	./scripts/elf2rbx $< $@

# ELF Compilation rules for applications
sdk/apps/desktop.elf: sdk/apps/desktop/main.o sdk/gui/src/window.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(desktop_ADDR) -e _start -o $@ $^

sdk/apps/taskbar.elf: sdk/apps/taskbar/main.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(taskbar_ADDR) -e rbx_module_init -o $@ $^

sdk/apps/startmenu.elf: sdk/apps/startmenu/main.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(startmenu_ADDR) -e rbx_module_init -o $@ $^

sdk/apps/statusbars.elf: sdk/apps/statusbars/main.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(statusbars_ADDR) -e rbx_module_init -o $@ $^

sdk/apps/calculator.elf: sdk/apps/calculator/main.o sdk/gui/src/apps.o sdk/gui/src/window.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(calculator_ADDR) -e rbx_module_init -o $@ $^

sdk/apps/texteditor.elf: sdk/apps/texteditor/main.o sdk/gui/src/apps.o sdk/gui/src/window.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(texteditor_ADDR) -e rbx_module_init -o $@ $^

sdk/apps/filemanager.elf: sdk/apps/filemanager/main.o sdk/gui/src/apps.o sdk/gui/src/window.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(filemanager_ADDR) -e rbx_module_init -o $@ $^

sdk/apps/calendar.elf: sdk/apps/calendar/main.o sdk/gui/src/apps.o sdk/gui/src/window.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(calendar_ADDR) -e rbx_module_init -o $@ $^

sdk/apps/settings.elf: sdk/apps/settings/main.o sdk/gui/src/apps.o sdk/gui/src/window.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(settings_ADDR) -e rbx_module_init -o $@ $^

sdk/apps/clock.elf: sdk/apps/clock/main.o sdk/gui/src/apps.o sdk/gui/src/window.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(clock_ADDR) -e rbx_module_init -o $@ $^

sdk/apps/sysmon.elf: sdk/apps/sysmon/main.o sdk/gui/src/apps.o sdk/gui/src/window.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(sysmon_ADDR) -e rbx_module_init -o $@ $^

sdk/apps/terminal.elf: sdk/apps/terminal/main.o sdk/gui/src/window.o sdk/framework/src/app_shim.o kernel/src/string.o
	$(LD) $(LDFLAGS_APP) -Ttext $(terminal_ADDR) -e rbx_module_init -o $@ $^

$(ISO): $(BIN)
	mkdir -p iso_root/boot/grub
	cp $(BIN) iso_root/boot/
	cp boot/grub.cfg iso_root/boot/grub/
	grub-mkrescue -d /usr/lib/grub/i386-pc -o $(ISO) iso_root
	rm -rf iso_root

clean:
	rm -f $(OBJ) $(BIN) $(ISO)
	rm -f sdk/apps/*.elf sdk/apps/*.rbx
	rm -f sdk/apps/*/*.o sdk/gui/src/*.o sdk/framework/src/*.o sdk/api/src/*.o
	rm -f sdk/framework/src/embedded_apps.cpp
	rm -f scripts/bin2c scripts/elf2rbx

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO)

.PHONY: all clean run

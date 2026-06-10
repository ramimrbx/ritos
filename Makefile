CC = gcc
CXX = g++
AS = gcc
LD = ld

NOGUI ?= 0

# All build artifacts live under build/, mirroring the source tree layout
# (e.g. kernel/src/fb.c -> build/kernel/src/fb.o) so names can never collide.
BUILD = build
GEN   = $(BUILD)/generated
TOOLS = $(BUILD)/tools

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic -fno-stack-protector -Ikernel/include -nostdlib -MMD -MP
CXXFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-pie -fno-pic -fno-stack-protector -Ikernel/include -Isdk/framework/include -Isdk/api/include -Isdk/gui/include -nostdlib -MMD -MP
ASFLAGS = -m32 -c
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib -no-pie
LDFLAGS_APP = -m elf_i386 -nostdlib

ifeq ($(NOGUI),1)
CXXFLAGS += -DNOGUI=1
endif

# App Addresses
desktop_ADDR    = 0x2000040
taskbar_ADDR    = 0x2100040
startmenu_ADDR  = 0x2200040
statusbars_ADDR = 0x2300040
calculator_ADDR = 0x2400040
texteditor_ADDR = 0x2500040
filemanager_ADDR= 0x2600040
calendar_ADDR   = 0x2700040
settings_ADDR   = 0x2800040
clock_ADDR      = 0x2900040
sysmon_ADDR     = 0x2A00040
terminal_ADDR   = 0x2B00040
imgview_ADDR    = 0x2C00040
nogui_shell_ADDR= 0x2000040

# App Targets
ifeq ($(NOGUI),1)
APPS = nogui_shell calculator texteditor filemanager calendar settings clock sysmon terminal
else
APPS = desktop taskbar startmenu statusbars calculator texteditor filemanager calendar settings clock sysmon terminal imgview
endif

APPS_RBX = $(foreach app,$(APPS),$(BUILD)/sdk/apps/$(app).rbx)

# Kernel files
C_SOURCES = $(wildcard kernel/src/*.c)
KERNEL_CPP_SOURCES = sdk/framework/src/cpp_support.cpp \
                     sdk/framework/src/system.cpp \
                     sdk/framework/src/vfs.cpp \
                     sdk/api/src/api.cpp \
                     sdk/api/src/sdk_entry.cpp

OBJ = $(BUILD)/boot/boot.o \
      $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES)) \
      $(patsubst %.cpp,$(BUILD)/%.o,$(KERNEL_CPP_SOURCES)) \
      $(GEN)/embedded_apps.o

# Objects shared by the apps
APP_SHIM   = $(BUILD)/sdk/framework/src/app_shim.o $(BUILD)/kernel/src/string.o
GUI_WINDOW = $(BUILD)/sdk/gui/src/window.o
GUI_APPS   = $(BUILD)/sdk/gui/src/apps.o $(GUI_WINDOW)

BIN = $(BUILD)/ritos.bin
ISO = $(BUILD)/ritos.iso

all: assets/font/font_8x16.h assets/icons/icons_32.h $(BIN) $(ISO)

assets/font/font_8x16.h:
	python3 scripts/gen_font.py

assets/icons/icons_32.h:
	python3 scripts/gen_icons.py

$(BIN): $(OBJ) linker.ld
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

# ── Generic compile rules (mirror source path under build/) ───────────────
$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/boot/boot.o: boot/boot.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $< -o $@

# ── Host tools ─────────────────────────────────────────────────────────────
$(TOOLS)/bin2c: scripts/bin2c.cpp
	@mkdir -p $(@D)
	g++ -O2 $< -o $@

$(TOOLS)/elf2rbx: scripts/elf2rbx.cpp
	@mkdir -p $(@D)
	g++ -O2 $< -o $@

# ── VFS embedding (generated source lives under build/generated) ──────────
$(GEN)/embedded_apps.cpp: $(APPS_RBX) $(TOOLS)/bin2c
	@mkdir -p $(@D)
	$(TOOLS)/bin2c $(BUILD)/sdk/apps $@

$(GEN)/embedded_apps.o: $(GEN)/embedded_apps.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ── RBX packaging ──────────────────────────────────────────────────────────
$(BUILD)/sdk/apps/%.rbx: $(BUILD)/sdk/apps/%.elf $(TOOLS)/elf2rbx
	$(TOOLS)/elf2rbx $< $@

# ── ELF link rules for applications ───────────────────────────────────────
$(BUILD)/sdk/apps/nogui_shell.elf: $(BUILD)/sdk/apps/nogui_shell/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(nogui_shell_ADDR) -e _start -o $@ $^

$(BUILD)/sdk/apps/desktop.elf: $(BUILD)/sdk/apps/desktop/main.o $(GUI_WINDOW) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(desktop_ADDR) -e _start -o $@ $^

$(BUILD)/sdk/apps/taskbar.elf: $(BUILD)/sdk/apps/taskbar/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(taskbar_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/startmenu.elf: $(BUILD)/sdk/apps/startmenu/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(startmenu_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/statusbars.elf: $(BUILD)/sdk/apps/statusbars/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(statusbars_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/calculator.elf: $(BUILD)/sdk/apps/calculator/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(calculator_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/texteditor.elf: $(BUILD)/sdk/apps/texteditor/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(texteditor_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/filemanager.elf: $(BUILD)/sdk/apps/filemanager/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(filemanager_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/calendar.elf: $(BUILD)/sdk/apps/calendar/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(calendar_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/settings.elf: $(BUILD)/sdk/apps/settings/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(settings_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/clock.elf: $(BUILD)/sdk/apps/clock/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(clock_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/sysmon.elf: $(BUILD)/sdk/apps/sysmon/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(sysmon_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/terminal.elf: $(BUILD)/sdk/apps/terminal/main.o $(GUI_WINDOW) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(terminal_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/sdk/apps/imgview.elf: $(BUILD)/sdk/apps/imgview/main.o $(GUI_WINDOW) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(imgview_ADDR) -e rbx_module_init -o $@ $^

# ── ISO image ──────────────────────────────────────────────────────────────
$(ISO): $(BIN) boot/grub.cfg
	mkdir -p $(BUILD)/iso_root/boot/grub
	cp $(BIN) $(BUILD)/iso_root/boot/
	cp boot/grub.cfg $(BUILD)/iso_root/boot/grub/
	grub-mkrescue -d /usr/lib/grub/i386-pc -o $(ISO) $(BUILD)/iso_root
	rm -rf $(BUILD)/iso_root

clean:
	rm -rf $(BUILD)

clean-assets:
	rm -f assets/font/font_8x16.h assets/icons/icons_32.h

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO)

gui:
	$(MAKE) clean
	$(MAKE) NOGUI=0

no-gui:
	$(MAKE) clean
	$(MAKE) NOGUI=1

# Auto-generated header dependency files (from -MMD)
-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)

.PHONY: all clean clean-assets run gui no-gui

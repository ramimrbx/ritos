# RitOS top-level build. Portable layers (kernel core, userspace/) are
# compiled the same way for every port; everything CPU/board specific comes
# from architecture/$(ARCH)/architecture.mk (toolchain, flags, boot code,
# image packaging).
ARCH ?= x86

NOGUI ?= 0

.DEFAULT_GOAL := all

# All build artifacts live under build/, mirroring the source tree layout:
# objects (kernel/drivers/framebuffer.c -> build/kernel/drivers/framebuffer.o)
# and app binaries (userspace/applications/clock/ ->
# build/userspace/applications/clock/clock.{elf,rbx}) alike, so two sources
# with the same name can never collide in the build tree.
BUILD = build
GEN   = $(BUILD)/generated
TOOLS = $(BUILD)/tools
BIN   = $(BUILD)/ritos.bin

include architecture/$(ARCH)/architecture.mk

# Include roots: architecture headers first so a port can provide/override
# <kernel/...> headers (e.g. input_output.h), then portable kernel headers,
# generated asset headers, then the userspace layers in dependency order.
INC_C   = -Iarchitecture/$(ARCH)/include -Ikernel/include -Iassets/generated
INC_CXX = $(INC_C) -Iuserspace/interface/include -Iuserspace/libraries/rit/include -Iuserspace/libraries/graphical_interface/include

CFLAGS = $(TARGET_CFLAGS) -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic -fno-stack-protector $(INC_C) -nostdlib -MMD -MP
CXXFLAGS = $(TARGET_CFLAGS) -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-pie -fno-pic -fno-stack-protector $(INC_CXX) -nostdlib -MMD -MP
ASFLAGS = $(TARGET_ASFLAGS)
LDFLAGS = -m $(TARGET_LDEMU) -T $(LINKER_SCRIPT) -nostdlib -no-pie
LDFLAGS_APP = -m $(TARGET_LDEMU) -nostdlib

ifeq ($(NOGUI),1)
CXXFLAGS += -DNOGUI=1
endif

# App Addresses
desktop_ADDR        = 0x2000040
taskbar_ADDR        = 0x2100040
start_menu_ADDR     = 0x2200040
status_bars_ADDR    = 0x2300040
calculator_ADDR     = 0x2400040
text_editor_ADDR    = 0x2500040
file_manager_ADDR   = 0x2600040
calendar_ADDR       = 0x2700040
settings_ADDR       = 0x2800040
clock_ADDR          = 0x2900040
system_monitor_ADDR = 0x2A00040
terminal_ADDR       = 0x2B00040
image_viewer_ADDR   = 0x2C00040
console_shell_ADDR  = 0x2000040

# App Targets (shell components live in userspace/shell/, ordinary
# applications in userspace/applications/)
ifeq ($(NOGUI),1)
APPS = console_shell calculator text_editor file_manager calendar settings clock system_monitor terminal
else
APPS = desktop taskbar start_menu calculator text_editor file_manager calendar settings clock system_monitor terminal image_viewer
endif

# Each app builds in its own mirrored directory.
SHELL_APPS = desktop taskbar start_menu
app_dir  = userspace/$(if $(filter $(1),$(SHELL_APPS)),shell,applications)/$(1)
APPS_RBX = $(foreach app,$(APPS),$(BUILD)/$(call app_dir,$(app))/$(app).rbx)

# Where each app installs inside the OS filesystem: shell components (and
# the console shell) under /system/shell, everything else under
# /system/executables.
app_vfs_dir = /system/$(if $(filter $(1),$(SHELL_APPS) console_shell),shell,executables)

# Shortcuts (.stct): tiny files whose content is the target executable path.
# Desktop icons come from /users/ramim/desktop, start-menu entries from
# /users/ramim/launcher. Underscores in names display as spaces.
DESKTOP_SHORTCUTS  = System_Monitor=system_monitor Calculator=calculator Text_Editor=text_editor File_Manager=file_manager Clock=clock Calendar=calendar Settings=settings Terminal=terminal
LAUNCHER_SHORTCUTS = $(DESKTOP_SHORTCUTS) Image_Viewer=image_viewer

stct_name = $(word 1,$(subst =, ,$(1)))
stct_app  = $(word 2,$(subst =, ,$(1)))
STCT_DIR  = $(BUILD)/shortcuts

# Everything embed_filesystem packs into the VFS image, as
# <vfs-path>=<built-file>
EMBED_FILES = $(foreach app,$(APPS),$(call app_vfs_dir,$(app))/$(app).rbx=$(BUILD)/$(call app_dir,$(app))/$(app).rbx) \
              $(foreach s,$(DESKTOP_SHORTCUTS),/users/ramim/desktop/$(call stct_name,$(s)).stct=$(STCT_DIR)/desktop/$(call stct_name,$(s)).stct) \
              $(foreach s,$(LAUNCHER_SHORTCUTS),/users/ramim/launcher/$(call stct_name,$(s)).stct=$(STCT_DIR)/launcher/$(call stct_name,$(s)).stct)

# Kernel files: portable core + the port's drivers
C_SOURCES = $(wildcard kernel/core/*.c kernel/drivers/*.c kernel/library/*.c) \
            $(ARCH_C_SOURCES)
KERNEL_CPP_SOURCES = userspace/runtime/runtime_support.cpp \
                     userspace/runtime/system_startup.cpp \
                     userspace/libraries/rit/source/system.cpp \
                     userspace/libraries/rit/source/virtual_filesystem.cpp \
                     userspace/interface/source/interface.cpp

OBJ = $(ARCH_OBJS) \
      $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES)) \
      $(patsubst %.cpp,$(BUILD)/%.o,$(KERNEL_CPP_SOURCES)) \
      $(GEN)/embedded_apps.o \
      $(GEN)/wallpaper.o

# Objects shared by the apps
APP_SHIM   = $(BUILD)/userspace/runtime/application_shim.o $(BUILD)/kernel/library/string.o
GUI_WINDOW = $(BUILD)/userspace/libraries/graphical_interface/source/window.o
GUI_APPS   = $(BUILD)/userspace/libraries/graphical_interface/source/application_windows.o $(GUI_WINDOW)

all: assets/generated/font_8x16.h assets/generated/icons_32.h assets/generated/instrument_font.h $(BIN) $(IMAGE)

assets/generated/font_8x16.h:
	python3 tools/generate_font.py

# Instrument Sans system font, baked to AA coverage atlases at build time
assets/generated/instrument_font.h: tools/generate_system_font.py
	python3 tools/generate_system_font.py

assets/generated/icons_32.h:
	python3 tools/generate_icons.py

$(BIN): $(OBJ) $(LINKER_SCRIPT)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

# ── Generic compile rules (mirror source path under build/) ───────────────
$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $< -o $@

# ── Host tools ─────────────────────────────────────────────────────────────
$(TOOLS)/embed_filesystem: tools/embed_filesystem.cpp
	@mkdir -p $(@D)
	g++ -O2 $< -o $@

$(TOOLS)/elf_to_rbx: tools/elf_to_rbx.cpp userspace/libraries/rit/include/rit/rbx_format.h
	@mkdir -p $(@D)
	g++ -O2 -Iuserspace/libraries/rit/include $< -o $@

# ── Shortcut files ─────────────────────────────────────────────────────────
$(STCT_DIR)/.stamp: Makefile
	@mkdir -p $(STCT_DIR)/desktop $(STCT_DIR)/launcher
	@$(foreach s,$(DESKTOP_SHORTCUTS),printf '%s' '/system/executables/$(call stct_app,$(s)).rbx' > $(STCT_DIR)/desktop/$(call stct_name,$(s)).stct;)
	@$(foreach s,$(LAUNCHER_SHORTCUTS),printf '%s' '/system/executables/$(call stct_app,$(s)).rbx' > $(STCT_DIR)/launcher/$(call stct_name,$(s)).stct;)
	@touch $@

# ── VFS embedding (generated source lives under build/generated) ──────────
$(GEN)/embedded_apps.cpp: $(APPS_RBX) $(STCT_DIR)/.stamp $(TOOLS)/embed_filesystem
	@mkdir -p $(@D)
	$(TOOLS)/embed_filesystem $@ $(EMBED_FILES)

$(GEN)/embedded_apps.o: $(GEN)/embedded_apps.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ── Default wallpaper (embedded into the kernel as a raw ARGB blob) ───────
$(GEN)/wallpaper.s: tools/generate_wallpaper.py $(wildcard assets/sources/wallpapers/*)
	python3 tools/generate_wallpaper.py

$(GEN)/wallpaper.o: $(GEN)/wallpaper.s
	$(AS) $(ASFLAGS) $< -o $@

# ── RBX packaging (next to the .elf in the app's build directory) ─────────
# Most apps are GUI modules; desktop and console_shell are standalone
# programs entered via _start (see RBX_TYPE_* in rit/rbx_format.h).
$(BUILD)/userspace/shell/desktop/desktop.rbx:                   RBX_TYPE = program
$(BUILD)/userspace/applications/console_shell/console_shell.rbx: RBX_TYPE = program

# Apps with an icon get it embedded in their .rbx (32x32 ARGB)
ICON_APPS = system_monitor calculator text_editor file_manager clock calendar settings terminal image_viewer
define APP_ICON_template
$(BUILD)/$(call app_dir,$(1))/$(1).rbx: RBX_ICON = assets/generated/icons/$(1).argb
$(BUILD)/$(call app_dir,$(1))/$(1).rbx: assets/generated/icons/$(1).argb
endef
$(foreach app,$(ICON_APPS),$(eval $(call APP_ICON_template,$(app))))

assets/generated/icons/%.argb:
	python3 tools/generate_icons.py

$(BUILD)/%.rbx: $(BUILD)/%.elf $(TOOLS)/elf_to_rbx
	$(TOOLS)/elf_to_rbx --type=$(or $(RBX_TYPE),module) $(if $(RBX_ICON),--icon=$(RBX_ICON)) $< $@

# ── ELF link rules for shell components (userspace/shell/) ────────────────
$(BUILD)/userspace/shell/desktop/desktop.elf: $(BUILD)/userspace/shell/desktop/main.o $(GUI_WINDOW) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(desktop_ADDR) -e _start -o $@ $^

$(BUILD)/userspace/shell/taskbar/taskbar.elf: $(BUILD)/userspace/shell/taskbar/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(taskbar_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/userspace/shell/start_menu/start_menu.elf: $(BUILD)/userspace/shell/start_menu/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(start_menu_ADDR) -e rbx_module_init -o $@ $^

# ── ELF link rules for applications (userspace/applications/) ─────────────
$(BUILD)/userspace/applications/console_shell/console_shell.elf: $(BUILD)/userspace/applications/console_shell/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(console_shell_ADDR) -e _start -o $@ $^

$(BUILD)/userspace/applications/calculator/calculator.elf: $(BUILD)/userspace/applications/calculator/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(calculator_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/userspace/applications/text_editor/text_editor.elf: $(BUILD)/userspace/applications/text_editor/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(text_editor_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/userspace/applications/file_manager/file_manager.elf: $(BUILD)/userspace/applications/file_manager/main.o $(GUI_WINDOW) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(file_manager_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/userspace/applications/calendar/calendar.elf: $(BUILD)/userspace/applications/calendar/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(calendar_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/userspace/applications/settings/settings.elf: $(BUILD)/userspace/applications/settings/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(settings_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/userspace/applications/clock/clock.elf: $(BUILD)/userspace/applications/clock/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(clock_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/userspace/applications/system_monitor/system_monitor.elf: $(BUILD)/userspace/applications/system_monitor/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(system_monitor_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/userspace/applications/terminal/terminal.elf: $(BUILD)/userspace/applications/terminal/main.o $(GUI_WINDOW) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(terminal_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/userspace/applications/image_viewer/image_viewer.elf: $(BUILD)/userspace/applications/image_viewer/main.o $(GUI_WINDOW) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(image_viewer_ADDR) -e rbx_module_init -o $@ $^

clean:
	rm -rf $(BUILD)

clean-assets:
	rm -f assets/generated/font_8x16.h assets/generated/icons_32.h

run: $(IMAGE)
	$(QEMU) $(IMAGE)

gui:
	$(MAKE) clean
	$(MAKE) NOGUI=0

no-gui:
	$(MAKE) clean
	$(MAKE) NOGUI=1

# Auto-generated header dependency files (from -MMD)
-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)

.PHONY: all clean clean-assets run gui no-gui

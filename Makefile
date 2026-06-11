# RitOS top-level build. Portable layers (kernel core, user/) are compiled
# the same way for every port; everything CPU/board specific comes from
# arch/$(ARCH)/arch.mk (toolchain, flags, boot code, image packaging).
ARCH ?= x86

NOGUI ?= 0

.DEFAULT_GOAL := all

# All build artifacts live under build/, mirroring the source tree layout:
# objects (kernel/drivers/fb.c -> build/kernel/drivers/fb.o) and app binaries
# (user/apps/clock/ -> build/user/apps/clock/clock.{elf,rbx}) alike, so two
# sources with the same name can never collide in the build tree.
BUILD = build
GEN   = $(BUILD)/generated
TOOLS = $(BUILD)/tools
BIN   = $(BUILD)/ritos.bin

include arch/$(ARCH)/arch.mk

# Include roots: arch headers first so a port can provide/override
# <kernel/...> headers (e.g. io.h), then portable kernel headers, generated
# asset headers, then the userland layers in dependency order.
INC_C   = -Iarch/$(ARCH)/include -Ikernel/include -Iassets/gen
INC_CXX = $(INC_C) -Iuser/api/include -Iuser/lib/rit/include -Iuser/lib/gui/include

CFLAGS = $(TARGET_CFLAGS) -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic -fno-stack-protector $(INC_C) -nostdlib -MMD -MP
CXXFLAGS = $(TARGET_CFLAGS) -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-pie -fno-pic -fno-stack-protector $(INC_CXX) -nostdlib -MMD -MP
ASFLAGS = $(TARGET_ASFLAGS)
LDFLAGS = -m $(TARGET_LDEMU) -T $(LINKER_SCRIPT) -nostdlib -no-pie
LDFLAGS_APP = -m $(TARGET_LDEMU) -nostdlib

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

# App Targets (shell apps live in user/shell/, ordinary apps in user/apps/)
ifeq ($(NOGUI),1)
APPS = nogui_shell calculator texteditor filemanager calendar settings clock sysmon terminal
else
APPS = desktop taskbar startmenu statusbars calculator texteditor filemanager calendar settings clock sysmon terminal imgview
endif

# Each app builds in its own mirrored directory. Shell components live in
# user/shell/, ordinary apps in user/apps/.
SHELL_APPS = desktop taskbar startmenu statusbars
app_dir  = user/$(if $(filter $(1),$(SHELL_APPS)),shell,apps)/$(1)
APPS_RBX = $(foreach app,$(APPS),$(BUILD)/$(call app_dir,$(app))/$(app).rbx)

# Shortcuts (.stct): tiny files whose content is the target executable path.
# Desktop icons come from /desktop, start-menu entries from /launcher.
DESKTOP_SHORTCUTS  = SysMon=sysmon Calc=calculator Editor=texteditor Files=filemanager Clock=clock Calendar=calendar Settings=settings Terminal=terminal
LAUNCHER_SHORTCUTS = $(DESKTOP_SHORTCUTS) ImgView=imgview

stct_name = $(word 1,$(subst =, ,$(1)))
stct_app  = $(word 2,$(subst =, ,$(1)))
STCT_DIR  = $(BUILD)/shortcuts

# Everything bin2c embeds into the VFS image, as <vfs-path>=<built-file>
EMBED_FILES = $(foreach app,$(APPS),/sys/$(app).rbx=$(BUILD)/$(call app_dir,$(app))/$(app).rbx) \
              $(foreach s,$(DESKTOP_SHORTCUTS),/desktop/$(call stct_name,$(s)).stct=$(STCT_DIR)/desktop/$(call stct_name,$(s)).stct) \
              $(foreach s,$(LAUNCHER_SHORTCUTS),/launcher/$(call stct_name,$(s)).stct=$(STCT_DIR)/launcher/$(call stct_name,$(s)).stct)

# Kernel files: portable core + the port's drivers
C_SOURCES = $(wildcard kernel/core/*.c kernel/drivers/*.c kernel/lib/*.c) \
            $(ARCH_C_SOURCES)
KERNEL_CPP_SOURCES = user/runtime/cpp_support.cpp \
                     user/runtime/sdk_entry.cpp \
                     user/lib/rit/src/system.cpp \
                     user/lib/rit/src/vfs.cpp \
                     user/api/src/api.cpp

OBJ = $(ARCH_OBJS) \
      $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES)) \
      $(patsubst %.cpp,$(BUILD)/%.o,$(KERNEL_CPP_SOURCES)) \
      $(GEN)/embedded_apps.o

# Objects shared by the apps
APP_SHIM   = $(BUILD)/user/runtime/app_shim.o $(BUILD)/kernel/lib/string.o
GUI_WINDOW = $(BUILD)/user/lib/gui/src/window.o
GUI_APPS   = $(BUILD)/user/lib/gui/src/apps.o $(GUI_WINDOW)

all: assets/gen/font_8x16.h assets/gen/icons_32.h $(BIN) $(IMAGE)

assets/gen/font_8x16.h:
	python3 tools/gen_font.py

assets/gen/icons_32.h:
	python3 tools/gen_icons.py

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
$(TOOLS)/bin2c: tools/bin2c.cpp
	@mkdir -p $(@D)
	g++ -O2 $< -o $@

$(TOOLS)/elf2rbx: tools/elf2rbx.cpp user/lib/rit/include/rit/rbx_format.h
	@mkdir -p $(@D)
	g++ -O2 -Iuser/lib/rit/include $< -o $@

# ── Shortcut files ─────────────────────────────────────────────────────────
$(STCT_DIR)/.stamp: Makefile
	@mkdir -p $(STCT_DIR)/desktop $(STCT_DIR)/launcher
	@$(foreach s,$(DESKTOP_SHORTCUTS),printf '%s' '/sys/$(call stct_app,$(s)).rbx' > $(STCT_DIR)/desktop/$(call stct_name,$(s)).stct;)
	@$(foreach s,$(LAUNCHER_SHORTCUTS),printf '%s' '/sys/$(call stct_app,$(s)).rbx' > $(STCT_DIR)/launcher/$(call stct_name,$(s)).stct;)
	@touch $@

# ── VFS embedding (generated source lives under build/generated) ──────────
$(GEN)/embedded_apps.cpp: $(APPS_RBX) $(STCT_DIR)/.stamp $(TOOLS)/bin2c
	@mkdir -p $(@D)
	$(TOOLS)/bin2c $@ $(EMBED_FILES)

$(GEN)/embedded_apps.o: $(GEN)/embedded_apps.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ── RBX packaging (next to the .elf in the app's build directory) ─────────
# Most apps are GUI modules; desktop and nogui_shell are standalone programs
# entered via _start (see RBX_TYPE_* in rit/rbx_format.h).
$(BUILD)/user/shell/desktop/desktop.rbx:        RBX_TYPE = program
$(BUILD)/user/apps/nogui_shell/nogui_shell.rbx: RBX_TYPE = program

# Apps with an icon get it embedded in their .rbx (32x32 ARGB)
ICON_APPS = sysmon calculator texteditor filemanager clock calendar settings terminal imgview
define APP_ICON_template
$(BUILD)/$(call app_dir,$(1))/$(1).rbx: RBX_ICON = assets/gen/icons/$(1).argb
$(BUILD)/$(call app_dir,$(1))/$(1).rbx: assets/gen/icons/$(1).argb
endef
$(foreach app,$(ICON_APPS),$(eval $(call APP_ICON_template,$(app))))

assets/gen/icons/%.argb:
	python3 tools/gen_icons.py

$(BUILD)/%.rbx: $(BUILD)/%.elf $(TOOLS)/elf2rbx
	$(TOOLS)/elf2rbx --type=$(or $(RBX_TYPE),module) $(if $(RBX_ICON),--icon=$(RBX_ICON)) $< $@

# ── ELF link rules for shell components (user/shell/) ─────────────────────
$(BUILD)/user/shell/desktop/desktop.elf: $(BUILD)/user/shell/desktop/main.o $(GUI_WINDOW) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(desktop_ADDR) -e _start -o $@ $^

$(BUILD)/user/shell/taskbar/taskbar.elf: $(BUILD)/user/shell/taskbar/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(taskbar_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/user/shell/startmenu/startmenu.elf: $(BUILD)/user/shell/startmenu/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(startmenu_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/user/shell/statusbars/statusbars.elf: $(BUILD)/user/shell/statusbars/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(statusbars_ADDR) -e rbx_module_init -o $@ $^

# ── ELF link rules for applications (user/apps/) ──────────────────────────
$(BUILD)/user/apps/nogui_shell/nogui_shell.elf: $(BUILD)/user/apps/nogui_shell/main.o $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(nogui_shell_ADDR) -e _start -o $@ $^

$(BUILD)/user/apps/calculator/calculator.elf: $(BUILD)/user/apps/calculator/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(calculator_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/user/apps/texteditor/texteditor.elf: $(BUILD)/user/apps/texteditor/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(texteditor_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/user/apps/filemanager/filemanager.elf: $(BUILD)/user/apps/filemanager/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(filemanager_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/user/apps/calendar/calendar.elf: $(BUILD)/user/apps/calendar/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(calendar_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/user/apps/settings/settings.elf: $(BUILD)/user/apps/settings/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(settings_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/user/apps/clock/clock.elf: $(BUILD)/user/apps/clock/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(clock_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/user/apps/sysmon/sysmon.elf: $(BUILD)/user/apps/sysmon/main.o $(GUI_APPS) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(sysmon_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/user/apps/terminal/terminal.elf: $(BUILD)/user/apps/terminal/main.o $(GUI_WINDOW) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(terminal_ADDR) -e rbx_module_init -o $@ $^

$(BUILD)/user/apps/imgview/imgview.elf: $(BUILD)/user/apps/imgview/main.o $(GUI_WINDOW) $(APP_SHIM)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS_APP) -Ttext $(imgview_ADDR) -e rbx_module_init -o $@ $^

clean:
	rm -rf $(BUILD)

clean-assets:
	rm -f assets/gen/font_8x16.h assets/gen/icons_32.h

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

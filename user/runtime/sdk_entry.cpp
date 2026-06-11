#include <ritos/api.hpp>
#include <rit/vfs.hpp>
#include <rit/system.hpp>
#include <kernel/io.h>

/* Serial debug helper (COM1) */
static void sdbg(const char* s) {
    while (*s) {
        while ((inb(0x3F8+5) & 0x20) == 0) {}
        outb(0x3F8, *s++);
    }
}

extern "C" {
	void ritos_init_framework() {
		ritos::init();
	}

	// Declared in generated embedded_apps.cpp
	void ritos_load_embedded_apps(void);

	void ritos_launch_gui() {
		sdbg("launch_gui: VFS init\r\n");
		// 1. Initialize Virtual File System
		rit::VFS::init();

		sdbg("launch_gui: load_embedded_apps\r\n");
		// 2. Load the embedded .rbx applications into the VFS
		ritos_load_embedded_apps();

		sdbg("launch_gui: init_mouse\r\n");
		// 3. Initialize mouse driver
		rit::System::init_mouse();

#ifdef NOGUI
		sdbg("launch_gui: load nogui_shell\r\n");
		void* entry = rit::System::load_rbx("/sys/nogui_shell.rbx");
		if (entry != nullptr) {
			typedef void (*shell_entry_t)(const RitOS_API* api);
			((shell_entry_t)entry)(rit::System::get_api_table());
		} else {
			rit::System::clear_screen();
			rit::System::println("CRITICAL ERROR: Failed to load /sys/nogui_shell.rbx!");
			while (1) { __asm__ volatile("cli; hlt"); }
		}
#else
		sdbg("launch_gui: load desktop.rbx\r\n");
		void* entry = rit::System::load_rbx("/sys/desktop.rbx");
		sdbg("launch_gui: desktop loaded, calling entry\r\n");
		if (entry != nullptr) {
			typedef void (*desktop_entry_t)(const RitOS_API* api);
			desktop_entry_t run_desktop = (desktop_entry_t)entry;
			sdbg("launch_gui: run_desktop calling\r\n");
			run_desktop(rit::System::get_api_table());
			sdbg("launch_gui: run_desktop returned\r\n");
		} else {
			sdbg("launch_gui: desktop.rbx load FAILED\r\n");
			rit::System::clear_screen();
			rit::System::println("CRITICAL ERROR: Failed to load /sys/desktop.rbx!");
			while (1) { __asm__ volatile("cli; hlt"); }
		}
#endif
	}
}

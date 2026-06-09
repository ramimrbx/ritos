#include "../include/ritos/api.hpp"
#include "../../framework/include/rit/vfs.hpp"
#include "../../framework/include/rit/system.hpp"

extern "C" {
	void ritos_init_framework() {
		ritos::init();
	}

	// Declared in generated embedded_apps.cpp
	void ritos_load_embedded_apps(void);

	void ritos_launch_gui() {
		// 1. Initialize Virtual File System
		rit::VFS::init();

		// 2. Load the embedded .rbx applications into the VFS
		ritos_load_embedded_apps();

		// 3. Initialize mouse driver
		rit::System::init_mouse();

		// 4. Load and launch desktop.rbx dynamic module
		void* entry = rit::System::load_rbx("/sys/desktop.rbx");
		if (entry != nullptr) {
			typedef void (*desktop_entry_t)(const RitOS_API* api);
			desktop_entry_t run_desktop = (desktop_entry_t)entry;
			run_desktop(rit::System::get_api_table());
		} else {
			// Print error message if we couldn't load it
			rit::System::clear_screen();
			rit::System::println("CRITICAL ERROR: Failed to load /sys/desktop.rbx!");
			while (1) {
				__asm__ volatile("cli; hlt");
			}
		}
	}
}

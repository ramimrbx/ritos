#include "../include/ritos/api.hpp"
#include "../../gui/include/ritos/desktop.hpp"
#include "../../gui/include/ritos/window.hpp"
#include "../../gui/include/ritos/apps.hpp"
#include "../../framework/include/rit/vfs.hpp"

extern "C" {
	void ritos_init_framework() {
		// Initialize the Ritos C++ SDK
		ritos::init();
	}

	void ritos_launch_gui() {
		// Initialize virtual file system
		rit::VFS::init();

		// Instantiate the C++ GUI Desktop
		ritos::Desktop desktop;

		// 1. Create System Monitor Window
		ritos::SystemMonitorWindow sys_mon_window(5, 3, 38, 10);
		sys_mon_window.set_visible(false);
		sys_mon_window.set_active(false);

		// 2. Create Calculator Window (initially hidden)
		ritos::CalculatorWindow calc_window(10, 4);
		calc_window.set_visible(false);
		calc_window.set_active(false);

		// 3. Create Text Editor Window (initially hidden)
		ritos::TextEditorWindow editor_window(20, 5, 40, 12);
		editor_window.set_visible(false);
		editor_window.set_active(false);

		// 4. Create File Explorer Window (initially visible)
		ritos::FileExplorerWindow file_explorer_window(15, 6, &editor_window);
		file_explorer_window.set_visible(false);
		file_explorer_window.set_active(false);

		// 5. Create Clock Window (initially hidden)
		ritos::ClockWindow clock_window(48, 4);
		clock_window.set_visible(false);
		clock_window.set_active(false);

		// 6. Create Calendar Window (initially hidden)
		ritos::CalendarWindow calendar_window(48, 12);
		calendar_window.set_visible(false);
		calendar_window.set_active(false);

		// 7. Create Settings Window (initially hidden)
		ritos::SettingsWindow settings_window(30, 8, &desktop);
		settings_window.set_visible(false);
		settings_window.set_active(false);

		// Add windows to desktop
		desktop.add_window(&sys_mon_window);
		desktop.add_window(&calc_window);
		desktop.add_window(&editor_window);
		desktop.add_window(&file_explorer_window);
		desktop.add_window(&clock_window);
		desktop.add_window(&calendar_window);
		desktop.add_window(&settings_window);

		// Initialize Mouse Driver
		rit::System::init_mouse();

		// Initial Draw
		desktop.draw();
		desktop.draw_cursor();

		int prev_mx = 40;
		int prev_my = 12;
		uint8_t prev_buttons = 0;

		// Track clock seconds to trigger periodic redraws
		int h = 0, m = 0, s = 0;
		rit::System::get_time(h, m, s);
		int prev_second = s;

		// Interactive GUI event loop
		while (1) {
			int mx = prev_mx;
			int my = prev_my;
			uint8_t buttons = prev_buttons;

			// 1. Poll Mouse
			if (rit::System::poll_mouse(mx, my, buttons)) {
				// Only redraw if state changed
				if (mx != prev_mx || my != prev_my || buttons != prev_buttons) {
					desktop.update_mouse(mx, my, buttons);
					desktop.draw();
					desktop.draw_cursor();
					
					prev_mx = mx;
					prev_my = my;
					prev_buttons = buttons;
				}
			}

			// 2. Poll Keyboard
			char key = 0;
			if (rit::System::poll_keyboard(key)) {
				if (key == '\t') {
					desktop.cycle_focus();
				} else if (key == 27) { // Escape key
					ritos::Window* active_win = desktop.get_active_window();
					if (active_win != nullptr) {
						active_win->set_visible(false);
						desktop.draw();
						desktop.draw_cursor();
					}
				} else if (key == '\n' && desktop.get_active_window() == nullptr) {
					desktop.trigger_focused_action();
				} else {
					// Route keyboard input to the active window
					ritos::Window* active_win = desktop.get_active_window();
					if (active_win != nullptr) {
						active_win->handle_key(key);
						desktop.draw();
						desktop.draw_cursor();
					}
				}
			}

			// 3. Update clock periodically (tick)
			int cur_h = 0, cur_m = 0, cur_s = 0;
			rit::System::get_time(cur_h, cur_m, cur_s);
			if (cur_s != prev_second) {
				desktop.draw();
				desktop.draw_cursor();
				prev_second = cur_s;
			}

			// Simple delay to throttle polling rate
			for (volatile uint32_t i = 0; i < 40000; i++) {
				__asm__ volatile("nop");
			}
		}
	}
}

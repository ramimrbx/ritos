#include "../../gui/include/ritos/window.hpp"
#include "../../framework/include/rit/system.hpp"
#include "../../framework/include/rit/rbx_module.h"

extern const RitOS_API* g_api;

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
	g_api = api;

	static rbx_module mod;
	mod.type = RBX_MODULE_STARTMENU;
	const char* nm = "Start Menu";
	int i = 0;
	while (nm[i] && i < 31) { mod.name[i] = nm[i]; i++; }
	mod.name[i] = '\0';
	mod.instance = (void*)desktop;

	mod.draw = [](void* inst) {
		const Desktop_Interface* d = static_cast<const Desktop_Interface*>(inst);
		int mx = d->get_mouse_x();
		int my = d->get_mouse_y();

		int sm_x = 1;
		int sm_y = 11;
		int sm_w = 19;
		int sm_h = 13;
		
		uint8_t dark_grey = (uint8_t)rit::Color::DarkGrey;
		uint8_t white = (uint8_t)rit::Color::White;

		g_api->draw_char('\xDA', white, dark_grey, sm_x, sm_y);
		for (int col = 1; col < sm_w - 1; col++) {
			g_api->draw_char('\xC4', white, dark_grey, sm_x + col, sm_y);
		}
		g_api->draw_char('\xBF', white, dark_grey, sm_x + sm_w - 1, sm_y);
		
		for (int row = 1; row < sm_h - 1; row++) {
			g_api->draw_char('\xB3', white, dark_grey, sm_x, sm_y + row);
			for (int col = 1; col < sm_w - 1; col++) {
				g_api->draw_char(' ', white, dark_grey, sm_x + col, sm_y + row);
			}
			g_api->draw_char('\xB3', white, dark_grey, sm_x + sm_w - 1, sm_y + row);
		}
		
		g_api->draw_char('\xC0', white, dark_grey, sm_x, sm_y + sm_h - 1);
		for (int col = 1; col < sm_w - 1; col++) {
			g_api->draw_char('\xC4', white, dark_grey, sm_x + col, sm_y + sm_h - 1);
		}
		g_api->draw_char('\xD9', white, dark_grey, sm_x + sm_w - 1, sm_y + sm_h - 1);
		
		const char* items[] = {
			"  Monitor",
			"  Calculator",
			"  Text Editor",
			"  File Explorer",
			"  Clock",
			"  Calendar",
			"  Settings",
			"  Terminal",
			" \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4",
			"  Restart",
			"  Shut Down"
		};
		
		for (int i = 0; i < 11; i++) {
			int item_y = sm_y + 1 + i;
			bool is_hovered = (mx >= sm_x + 1 && mx < sm_x + sm_w - 1 && my == item_y);
			if (i == 8) is_hovered = false;
			
			uint8_t bg = dark_grey;
			uint8_t fg = white;
			
			if (is_hovered) {
				bg = (uint8_t)rit::Color::Blue;
			}
			
			int col = 0;
			while (items[i][col] != '\0') {
				g_api->draw_char(items[i][col], fg, bg, sm_x + 1 + col, item_y);
				col++;
			}
			
			for (int x = col; x < sm_w - 2; x++) {
				g_api->draw_char(' ', fg, bg, sm_x + 1 + x, item_y);
			}
		}
	};

	mod.handle_click = [](void* inst, int mx, int my) {
		Desktop_Interface* d = static_cast<Desktop_Interface*>(inst);
		d->set_start_menu_open(0); // Close menu on click

		if (mx >= 1 && mx <= 19 && my >= 12 && my <= 22) {
			int idx = my - 12;
			const char* app_titles[] = {
				"System Monitor",
				"Calculator",
				"Text Editor",
				"File Explorer",
				"Clock",
				"Calendar",
				"Settings",
				"Terminal"
			};
			if (idx >= 0 && idx < 8) {
				d->launch_app(app_titles[idx]);
			} else if (idx == 9) { // Restart
				g_api->reboot();
			} else if (idx == 10) { // Shut down
				g_api->shutdown();
			}
		}
	};

	mod.handle_key = [](void* inst, char key) { (void)inst; (void)key; };

	return &mod;
}

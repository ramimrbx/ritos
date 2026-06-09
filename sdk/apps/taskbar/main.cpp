#include "../../gui/include/ritos/window.hpp"
#include "../../framework/include/rit/system.hpp"
#include "../../framework/include/rit/rbx_module.h"

extern const RitOS_API* g_api;

static bool str_equals(const char* s1, const char* s2) {
	int i = 0;
	while (s1[i] != '\0' && s2[i] != '\0') {
		if (s1[i] != s2[i]) return false;
		i++;
	}
	return s1[i] == s2[i];
}

static bool str_starts_with(const char* str, const char* prefix) {
	int i = 0;
	while (prefix[i] != '\0') {
		if (str[i] != prefix[i]) return false;
		i++;
	}
	return true;
}

static const char* get_short_label(const char* title) {
	if (str_starts_with(title, "System Monitor")) return "SysMon";
	if (str_starts_with(title, "Calculator")) return "Calc";
	if (str_starts_with(title, "Text Editor")) return "Editor";
	if (str_starts_with(title, "File Explorer")) return "Files";
	if (str_starts_with(title, "Clock")) return "Clock";
	if (str_starts_with(title, "Calendar")) return "Calen";
	if (str_starts_with(title, "Settings")) return "Config";
	if (str_starts_with(title, "Terminal")) return "Term";
	return title;
}

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
	g_api = api;

	static rbx_module mod;
	mod.type = RBX_MODULE_TASKBAR;
	const char* nm = "Task Bar";
	int i = 0;
	while (nm[i] && i < 31) { mod.name[i] = nm[i]; i++; }
	mod.name[i] = '\0';
	mod.instance = (void*)desktop;

	mod.draw = [](void* inst) {
		const Desktop_Interface* d = static_cast<const Desktop_Interface*>(inst);
		
		// 1. Draw taskbar background
		uint8_t bar_color = (uint8_t)rit::Color::LightGrey;
		for (int x = 0; x < 80; x++) {
			g_api->draw_char(' ', (uint8_t)rit::Color::Black, bar_color, x, 24);
		}

		// 2. Draw Start button
		int start_menu_open = d->is_start_menu_open();
		int mx = d->get_mouse_x();
		int my = d->get_mouse_y();
		
		uint8_t start_bg = start_menu_open ? (uint8_t)rit::Color::Blue : bar_color;
		uint8_t start_fg = start_menu_open ? (uint8_t)rit::Color::White : (uint8_t)rit::Color::Black;
		
		if (!start_menu_open && my == 24 && mx >= 1 && mx <= 11) {
			start_bg = (uint8_t)rit::Color::LightCyan;
		}
		
		const char* start_txt = " \x0F Start   ";
		for (int x = 0; start_txt[x] != '\0'; x++) {
			g_api->draw_char(start_txt[x], start_fg, start_bg, 1 + x, 24);
		}

		// 3. Draw dynamic taskbar buttons for open windows
		int win_count = d->get_window_count();
		int current_x = 13;
		for (int w = 0; w < win_count; w++) {
			if (!d->is_window_visible(w)) continue;

			const char* title = d->get_window_title(w);
			const char* label = get_short_label(title);
			int lbl_len = 0;
			while (label[lbl_len] != '\0') lbl_len++;
			int bw = lbl_len + 2;

			// Do not draw beyond the clock starting at x = 71
			if (current_x + bw > 70) break;

			int is_active = d->is_window_active(w);
			int is_minimized = d->is_window_minimized(w);
			int is_hovered = (my == 24 && mx >= current_x && mx < current_x + bw);

			uint8_t fg = (uint8_t)rit::Color::Black;
			uint8_t bg = bar_color;

			if (is_hovered) {
				bg = (uint8_t)rit::Color::LightCyan;
			} else if (is_active) {
				bg = (uint8_t)rit::Color::Blue;
				fg = (uint8_t)rit::Color::White;
			} else if (!is_minimized) {
				bg = (uint8_t)rit::Color::DarkGrey;
				fg = (uint8_t)rit::Color::White;
			}

			g_api->draw_char('[', fg, bg, current_x, 24);
			for (int char_idx = 0; char_idx < lbl_len; char_idx++) {
				g_api->draw_char(label[char_idx], fg, bg, current_x + 1 + char_idx, 24);
			}
			g_api->draw_char(']', fg, bg, current_x + 1 + lbl_len, 24);

			current_x += bw + 1; // 1 column space between buttons
		}

		// 4. Draw digital clock
		int h = 0, m = 0, s = 0;
		g_api->get_time(&h, &m, &s);
		char clock_buf[16];
		clock_buf[0] = (h / 10) + '0';
		clock_buf[1] = (h % 10) + '0';
		clock_buf[2] = ':';
		clock_buf[3] = (m / 10) + '0';
		clock_buf[4] = (m % 10) + '0';
		clock_buf[5] = ':';
		clock_buf[6] = (s / 10) + '0';
		clock_buf[7] = (s % 10) + '0';
		clock_buf[8] = '\0';
		for (int idx = 0; clock_buf[idx] != '\0'; idx++) {
			g_api->draw_char(clock_buf[idx], (uint8_t)rit::Color::Black, bar_color, 71 + idx, 24);
		}
	};

	mod.handle_click = [](void* inst, int mx, int my) {
		Desktop_Interface* d = static_cast<Desktop_Interface*>(inst);
		if (my != 24) return;

		// Start button click
		if (mx >= 1 && mx <= 11) {
			d->set_start_menu_open(!d->is_start_menu_open());
			return;
		}

		// Check dynamic window buttons
		int win_count = d->get_window_count();
		int current_x = 13;
		for (int w = 0; w < win_count; w++) {
			if (!d->is_window_visible(w)) continue;

			const char* title = d->get_window_title(w);
			const char* label = get_short_label(title);
			int lbl_len = 0;
			while (label[lbl_len] != '\0') lbl_len++;
			int bw = lbl_len + 2;

			if (current_x + bw > 70) break;

			if (mx >= current_x && mx < current_x + bw) {
				d->toggle_window(w);
				return;
			}

			current_x += bw + 1;
		}
	};

	mod.handle_key = [](void* inst, char key) { (void)inst; (void)key; };

	return &mod;
}

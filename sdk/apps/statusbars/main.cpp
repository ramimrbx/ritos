#include "../../gui/include/ritos/window.hpp"
#include "../../framework/include/rit/system.hpp"
#include "../../framework/include/rit/rbx_module.h"

extern const RitOS_API* g_api;

static void int_to_str(int n, char* buf) {
	int i = 0;
	bool is_neg = false;
	if (n < 0) { is_neg = true; n = -n; }
	if (n == 0) { buf[i++] = '0'; }
	else {
		while (n > 0) { buf[i++] = (n % 10) + '0'; n /= 10; }
	}
	if (is_neg) buf[i++] = '-';
	buf[i] = '\0';
	int len = i;
	for (int j = 0; j < len / 2; j++) {
		char temp = buf[j];
		buf[j] = buf[len - j - 1];
		buf[len - j - 1] = temp;
	}
}

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
	g_api = api;

	static rbx_module mod;
	mod.type = RBX_MODULE_STATUSBAR;
	const char* nm = "Status Bar";
	int i = 0;
	while (nm[i] && i < 31) { mod.name[i] = nm[i]; i++; }
	mod.name[i] = '\0';
	mod.instance = (void*)desktop;

	mod.draw = [](void* inst) {
		const Desktop_Interface* d = static_cast<const Desktop_Interface*>(inst);
		
		// 1. Draw background
		for (int x = 0; x < 80; x++) {
			g_api->draw_char(' ', (uint8_t)rit::Color::White, (uint8_t)rit::Color::DarkGrey, x, 0);
		}

		// 2. Draw Shortcuts text
		const char* shortcuts = " [Tab] Cycle Focus  |  [Esc] Close Active";
		for (int x = 0; shortcuts[x] != '\0'; x++) {
			g_api->draw_char(shortcuts[x], (uint8_t)rit::Color::LightGrey, (uint8_t)rit::Color::DarkGrey, 2 + x, 0);
		}

		// 3. Draw Active Window indicator
		int act_x = 44;
		const char* act_prefix = "Active: ";
		for (int x = 0; act_prefix[x] != '\0'; x++) {
			g_api->draw_char(act_prefix[x], (uint8_t)rit::Color::LightCyan, (uint8_t)rit::Color::DarkGrey, act_x + x, 0);
		}
		act_x += 8;

		// Find active window name
		const char* active_title = "None";
		int win_count = d->get_window_count();
		for (int w = 0; w < win_count; w++) {
			if (d->is_window_active(w)) {
				active_title = d->get_window_title(w);
				break;
			}
		}

		for (int x = 0; active_title[x] != '\0'; x++) {
			g_api->draw_char(active_title[x], (uint8_t)rit::Color::White, (uint8_t)rit::Color::DarkGrey, act_x + x, 0);
		}

		// 4. Draw Heap Memory usage (right-aligned)
		size_t heap_usage = g_api->get_heap_usage();
		char heap_buf[16];
		int_to_str(heap_usage, heap_buf);
		int hp_x = 78;
		g_api->draw_char('B', (uint8_t)rit::Color::LightGrey, (uint8_t)rit::Color::DarkGrey, hp_x--, 0);
		g_api->draw_char(' ', (uint8_t)rit::Color::LightGrey, (uint8_t)rit::Color::DarkGrey, hp_x--, 0);
		
		int h_idx = 0;
		while (heap_buf[h_idx] != '\0') h_idx++;
		for (int idx = h_idx - 1; idx >= 0; idx--) {
			g_api->draw_char(heap_buf[idx], (uint8_t)rit::Color::White, (uint8_t)rit::Color::DarkGrey, hp_x--, 0);
		}
		const char* mem_lbl = " Mem:";
		for (int idx = 4; idx >= 0; idx--) {
			g_api->draw_char(mem_lbl[idx], (uint8_t)rit::Color::LightCyan, (uint8_t)rit::Color::DarkGrey, hp_x--, 0);
		}
	};

	mod.handle_click = [](void* inst, int mx, int my) { (void)inst; (void)mx; (void)my; };
	mod.handle_key = [](void* inst, char key) { (void)inst; (void)key; };

	return &mod;
}

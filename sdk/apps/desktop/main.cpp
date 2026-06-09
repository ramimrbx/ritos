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

class RbxWindow : public ritos::Window {
private:
	rbx_module* m_mod;
	char m_rbx_path[64];
public:
	RbxWindow(rbx_module* mod, const char* path)
		: ritos::Window(mod->get_title(mod->instance), mod->get_x(mod->instance), mod->get_y(mod->instance), mod->get_width(mod->instance), mod->get_height(mod->instance)), m_mod(mod) {
		m_visible = mod->is_visible(mod->instance) != 0;
		m_minimized = mod->is_minimized(mod->instance) != 0;
		m_active = mod->is_active(mod->instance) != 0;
		int i = 0;
		while (path[i] && i < 63) { m_rbx_path[i] = path[i]; i++; }
		m_rbx_path[i] = '\0';
	}

	const char* get_rbx_path() const { return m_rbx_path; }
	rbx_module* get_module() const { return m_mod; }

	void draw() override {
		if (!m_visible || m_minimized) return;
		ritos::Window::draw();
		m_mod->draw(m_mod->instance);
	}

	void handle_click(int mx, int my) override {
		m_mod->handle_click(m_mod->instance, mx, my);
	}

	void handle_key(char key) override {
		m_mod->handle_key(m_mod->instance, key);
	}

	void set_position(int x, int y) override {
		ritos::Window::set_position(x, y);
		m_mod->set_position(m_mod->instance, x, y);
	}

	void set_visible(bool visible) override {
		ritos::Window::set_visible(visible);
		m_mod->set_visible(m_mod->instance, visible ? 1 : 0);
	}

	void set_minimized(bool minimized) override {
		ritos::Window::set_minimized(minimized);
		m_mod->set_minimized(m_mod->instance, minimized ? 1 : 0);
	}

	void set_active(bool active) override {
		ritos::Window::set_active(active);
		m_mod->set_active(m_mod->instance, active ? 1 : 0);
	}
};

class Desktop {
private:
	rit::Color m_bg_color;
	char m_bg_pattern;
	ritos::Window* m_windows[16];
	int m_window_count;

	int m_mouse_x;
	int m_mouse_y;
	uint8_t m_mouse_buttons;
	
	ritos::Window* m_dragged_window;
	int m_drag_offset_x;
	int m_drag_offset_y;
	
	bool m_start_menu_open;

	rbx_module* m_taskbar;
	rbx_module* m_startmenu;
	rbx_module* m_statusbar;

	void draw_icon(const char* label, const char* data, const uint8_t* colors, int x, int y) {
		for (int row = 0; row < 3; row++) {
			for (int col = 0; col < 4; col++) {
				char c = data[row * 4 + col];
				uint8_t fg = colors[row * 4 + col];
				g_api->draw_char(c, fg, (uint8_t)m_bg_color, x + col, y + row);
			}
		}
		for (int i = 0; label[i] != '\0'; i++) {
			g_api->draw_char(label[i], (uint8_t)rit::Color::White, (uint8_t)m_bg_color, x - 1 + i, y + 3);
		}
	}

public:
	Desktop()
		: m_bg_color(rit::Color::Blue), m_bg_pattern('G'), m_window_count(0),
		  m_mouse_x(40), m_mouse_y(12), m_mouse_buttons(0), m_dragged_window(nullptr),
		  m_drag_offset_x(0), m_drag_offset_y(0), m_start_menu_open(false),
		  m_taskbar(nullptr), m_startmenu(nullptr), m_statusbar(nullptr) {
		for (int i = 0; i < 16; i++) m_windows[i] = nullptr;
	}

	int get_window_count() const { return m_window_count; }
	ritos::Window* get_window(int idx) const { return m_windows[idx]; }
	
	int get_mouse_x() const { return m_mouse_x; }
	int get_mouse_y() const { return m_mouse_y; }
	int is_start_menu_open() const { return m_start_menu_open ? 1 : 0; }
	void set_start_menu_open(int open) { m_start_menu_open = (open != 0); }

	void add_window(ritos::Window* win) {
		if (m_window_count < 16) {
			m_windows[m_window_count++] = win;
		}
	}

	void focus_window(ritos::Window* win) {
		int idx = -1;
		for (int i = 0; i < m_window_count; i++) {
			if (m_windows[i] == win) {
				idx = i;
				break;
			}
		}
		if (idx >= 0) {
			for (int s = idx; s < m_window_count - 1; s++) {
				m_windows[s] = m_windows[s + 1];
			}
			m_windows[m_window_count - 1] = win;
		}
		for (int i = 0; i < m_window_count; i++) {
			if (m_windows[i]) {
				m_windows[i]->set_active(i == m_window_count - 1);
			}
		}
	}

	void toggle_window(int idx) {
		if (idx >= 0 && idx < m_window_count) {
			ritos::Window* win = m_windows[idx];
			if (!win->is_visible() || win->is_minimized()) {
				win->set_visible(true);
				win->set_minimized(false);
				focus_window(win);
			} else {
				if (win->is_active()) {
					win->set_minimized(true);
				} else {
					focus_window(win);
				}
			}
		}
	}

	ritos::Window* get_active_window() {
		for (int i = m_window_count - 1; i >= 0; i--) {
			if (m_windows[i] && m_windows[i]->is_visible() && !m_windows[i]->is_minimized() && m_windows[i]->is_active()) {
				return m_windows[i];
			}
		}
		return nullptr;
	}

	void cycle_focus() {
		if (m_window_count <= 0) return;
		
		// Find first visible window index
		int first_visible = -1;
		for (int i = 0; i < m_window_count; i++) {
			if (m_windows[i] && m_windows[i]->is_visible() && !m_windows[i]->is_minimized()) {
				first_visible = i;
				break;
			}
		}
		
		if (first_visible >= 0) {
			ritos::Window* win = m_windows[first_visible];
			// Cycle to the back of the queue (brings next to front)
			for (int i = first_visible; i < m_window_count - 1; i++) {
				m_windows[i] = m_windows[i + 1];
			}
			m_windows[m_window_count - 1] = win;
			
			for (int i = 0; i < m_window_count; i++) {
				if (m_windows[i]) {
					m_windows[i]->set_active(i == m_window_count - 1);
				}
			}
		}
	}

	void load_and_run_app(const char* rbx_path);

	void init_modules(const Desktop_Interface* d_int) {
		void* tb_entry = g_api->load_rbx("/sys/taskbar.rbx");
		if (tb_entry) {
			m_taskbar = ((rbx_init_t)tb_entry)(g_api, d_int);
		}
		void* sm_entry = g_api->load_rbx("/sys/startmenu.rbx");
		if (sm_entry) {
			m_startmenu = ((rbx_init_t)sm_entry)(g_api, d_int);
		}
		void* sb_entry = g_api->load_rbx("/sys/statusbars.rbx");
		if (sb_entry) {
			m_statusbar = ((rbx_init_t)sb_entry)(g_api, d_int);
		}
	}

	void update_mouse(int x, int y, uint8_t buttons) {
		bool lbutton_was_pressed = (m_mouse_buttons & 1) != 0;
		bool lbutton_is_pressed = (buttons & 1) != 0;

		m_mouse_x = x;
		m_mouse_y = y;
		m_mouse_buttons = buttons;

		if (!lbutton_was_pressed && lbutton_is_pressed) {
			handle_mouse_down(x, y);
		} else if (lbutton_was_pressed && !lbutton_is_pressed) {
			handle_mouse_up();
		} else if (lbutton_is_pressed) {
			handle_mouse_move(x, y);
		}
	}

	void handle_mouse_down(int x, int y) {
		if (m_start_menu_open) {
			if (x >= 1 && x <= 19 && y >= 12 && y <= 23) {
				if (m_startmenu) m_startmenu->handle_click(m_startmenu->instance, x, y);
				draw();
				return;
			}
			m_start_menu_open = false;
			draw();
		}

		if (y == 24) {
			if (m_taskbar) m_taskbar->handle_click(m_taskbar->instance, x, y);
			draw();
			return;
		}

		m_dragged_window = nullptr;
		for (int i = m_window_count - 1; i >= 0; i--) {
			ritos::Window* win = m_windows[i];
			if (win != nullptr && win->is_visible() && !win->is_minimized()) {
				int wx = win->get_x();
				int wy = win->get_y();
				int ww = win->get_width();
				int wh = win->get_height();
				
				if (x >= wx && x < wx + ww && y >= wy && y < wy + wh) {
					if (i < m_window_count - 1) {
						for (int k = i; k < m_window_count - 1; k++) {
							m_windows[k] = m_windows[k + 1];
						}
						m_windows[m_window_count - 1] = win;
					}
					
					for (int k = 0; k < m_window_count; k++) {
						if (m_windows[k]) {
							m_windows[k]->set_active(k == m_window_count - 1);
						}
					}
					
					if (y == wy) {
						if (x >= wx + ww - 3 && x <= wx + ww - 2) {
							win->set_visible(false);
						}
						else if (x >= wx + ww - 6 && x <= wx + ww - 5) {
							win->set_minimized(true);
						}
						else if (x >= wx + ww - 9 && x <= wx + ww - 7) {
							if (ww >= 11) {
								win->set_maximized(!win->is_maximized());
							}
						}
						else {
							if (!win->is_maximized()) {
								m_dragged_window = win;
								m_drag_offset_x = x - wx;
								m_drag_offset_y = y - wy;
							}
						}
					} else {
						win->handle_click(x, y);
					}
					draw();
					break;
				}
			}
		}

		// Column 1 (x: 1-8)
		if (x >= 1 && x <= 8) {
			if (y >= 2 && y <= 5) load_and_run_app("/sys/sysmon.rbx");
			else if (y >= 7 && y <= 10) load_and_run_app("/sys/calculator.rbx");
			else if (y >= 12 && y <= 15) load_and_run_app("/sys/texteditor.rbx");
			else if (y >= 17 && y <= 20) load_and_run_app("/sys/filemanager.rbx");
		}
		// Column 2 (x: 9-16)
		else if (x >= 9 && x <= 16) {
			if (y >= 2 && y <= 5) load_and_run_app("/sys/clock.rbx");
			else if (y >= 7 && y <= 10) load_and_run_app("/sys/calendar.rbx");
			else if (y >= 12 && y <= 15) load_and_run_app("/sys/settings.rbx");
			else if (y >= 17 && y <= 20) load_and_run_app("/sys/terminal.rbx");
		}
	}

	void handle_mouse_move(int x, int y) {
		if (m_dragged_window != nullptr) {
			if (m_dragged_window->is_maximized()) return;
			int new_x = x - m_drag_offset_x;
			int new_y = y - m_drag_offset_y;
			if (new_x < -10) new_x = -10;
			if (new_x > 70) new_x = 70;
			if (new_y < 1) new_y = 1;
			if (new_y > 23) new_y = 23;
			m_dragged_window->set_position(new_x, new_y);
			draw();
		}
	}

	void handle_mouse_up() {
		m_dragged_window = nullptr;
	}

	void draw() {
		if (g_api->vfs_exists("/sys/settings.cfg")) {
			int len = 0;
			const char* cfg = g_api->vfs_read_file("/sys/settings.cfg", &len);
			if (cfg && len >= 2) {
				m_bg_pattern = cfg[0];
				char col_char = cfg[1];
				if (col_char == 'B') m_bg_color = rit::Color::Blue;
				else if (col_char == 'D') m_bg_color = rit::Color::DarkGrey;
				else if (col_char == 'G') m_bg_color = rit::Color::Green;
				else if (col_char == 'R') m_bg_color = rit::Color::Red;
			}
		}

		for (int y = 1; y < 24; y++) {
			for (int x = 0; x < 80; x++) {
				if (m_bg_pattern == 'G') {
					if ((x % 4 == 0) && (y % 2 == 0)) {
						g_api->draw_char('.', (uint8_t)rit::Color::LightBlue, (uint8_t)m_bg_color, x, y);
					} else {
						g_api->draw_char(' ', (uint8_t)m_bg_color, (uint8_t)m_bg_color, x, y);
					}
				} else if (m_bg_pattern == '*') {
					if ((x * 7 + y * 13) % 43 == 0) {
						g_api->draw_char('*', (uint8_t)rit::Color::White, (uint8_t)m_bg_color, x, y);
					} else {
						g_api->draw_char(' ', (uint8_t)m_bg_color, (uint8_t)m_bg_color, x, y);
					}
				} else {
					g_api->draw_char(' ', (uint8_t)m_bg_color, (uint8_t)m_bg_color, x, y);
				}
			}
		}

		// Draw icons
		const char* mon_data = "\xDA\xC4\xC4\xBF\xB3\xDB\xDB\xB3\xC0\xC1\xC1\xD9";
		const uint8_t mon_cols[] = { 8, 8, 8, 8, 8, 11, 11, 8, 8, 8, 8, 8 };
		draw_icon("Monitor", mon_data, mon_cols, 2, 2);

		const char* calc_data = "\xDA\xC4\xC4\xBF\xB3\xF1\xF7\xB3\xC0\xC1\xC1\xD9";
		const uint8_t calc_cols[] = { 8, 8, 8, 8, 8, 14, 14, 8, 8, 8, 8, 8 };
		draw_icon(" Calc  ", calc_data, calc_cols, 2, 7);

		const char* edit_data = "\xDA\xC4\xC4\xBF\xB3\x0E\x0E\xB3\xC0\xC1\xC1\xD9";
		const uint8_t edit_cols[] = { 8, 8, 8, 8, 8, 10, 10, 8, 8, 8, 8, 8 };
		draw_icon("Editor ", edit_data, edit_cols, 2, 12);

		const char* files_data = "\xDA\xC4\xC4\xBF\xB3\x0E\x0E\xB3\xC0\xC1\xC1\xD9";
		const uint8_t files_cols[] = { 8, 8, 8, 8, 8, 14, 14, 8, 8, 8, 8, 8 };
		draw_icon("Files  ", files_data, files_cols, 2, 17);

		const char* clock_data = "\xDA\xC4\xC4\xBF\xB3\x18\x1A\xB3\xC0\xC1\xC1\xD9";
		const uint8_t clock_cols[] = { 8, 8, 8, 8, 8, 10, 10, 8, 8, 8, 8, 8 };
		draw_icon("Clock  ", clock_data, clock_cols, 10, 2);

		const char* cal_data = "\xDA\xC4\xC4\xBF\xB3\x33\x31\xB3\xC0\xC1\xC1\xD9";
		const uint8_t cal_cols[] = { 8, 8, 8, 8, 8, 12, 12, 8, 8, 8, 8, 8 };
		draw_icon("Calen  ", cal_data, cal_cols, 10, 7);

		const char* set_data = "\xDA\xC4\xC4\xBF\xB3\x0F\x0F\xB3\xC0\xC1\xC1\xD9";
		const uint8_t set_cols[] = { 8, 8, 8, 8, 8, 13, 13, 8, 8, 8, 8, 8 };
		draw_icon("Config ", set_data, set_cols, 10, 12);

		const char* term_data = "\xDA\xC4\xC4\xBF\xB3\x3E\x5F\xB3\xC0\xC1\xC1\xD9";
		const uint8_t term_cols[] = { 8, 8, 8, 8, 8, 10, 10, 8, 8, 8, 8, 8 };
		draw_icon("Term   ", term_data, term_cols, 10, 17);

		if (m_statusbar) m_statusbar->draw(m_statusbar->instance);

		for (int i = 0; i < m_window_count; i++) {
			if (m_windows[i]) m_windows[i]->draw();
		}

		if (m_taskbar) m_taskbar->draw(m_taskbar->instance);
		if (m_start_menu_open && m_startmenu) m_startmenu->draw(m_startmenu->instance);
	}

	void draw_cursor() {
		uint16_t* video_memory = g_api->get_screen_buffer();
		if (!video_memory) video_memory = (uint16_t*)0xB8000;
		int shadow_offsets[3][2] = { {1, 0}, {0, 1}, {1, 1} };
		for (int i = 0; i < 3; i++) {
			int sx = m_mouse_x + shadow_offsets[i][0];
			int sy = m_mouse_y + shadow_offsets[i][1];
			if (sx >= 0 && sx < 80 && sy >= 0 && sy < 25) {
				int idx = sy * 80 + sx;
				uint16_t cell = video_memory[idx];
				uint8_t character = cell & 0xFF;
				uint8_t attribute = (cell >> 8) & 0xFF;
				uint8_t fg = attribute & 0x0F;
				uint8_t new_attribute = fg | (8 << 4);
				video_memory[idx] = (uint16_t)character | ((uint16_t)new_attribute << 8);
			}
		}
		int index = m_mouse_y * 80 + m_mouse_x;
		if (index >= 0 && index < 80 * 25) {
			uint8_t character = 0x1E;
			uint8_t attribute = 0x0F | (11 << 4);
			video_memory[index] = (uint16_t)character | ((uint16_t)attribute << 8);
		}
	}
};

static Desktop* g_desktop = nullptr;

static int api_get_window_count() { return g_desktop->get_window_count(); }
static const char* api_get_window_title(int idx) { return g_desktop->get_window(idx)->get_title(); }
static int api_is_window_active(int idx) { return g_desktop->get_window(idx)->is_active() ? 1 : 0; }
static int api_is_window_visible(int idx) { return g_desktop->get_window(idx)->is_visible() ? 1 : 0; }
static int api_is_window_minimized(int idx) { return g_desktop->get_window(idx)->is_minimized() ? 1 : 0; }
static void api_toggle_window(int idx) { g_desktop->toggle_window(idx); }
static void api_launch_app(const char* title) { g_desktop->load_and_run_app(title); }
static int api_get_mouse_x() { return g_desktop->get_mouse_x(); }
static int api_get_mouse_y() { return g_desktop->get_mouse_y(); }
static int api_is_start_menu_open() { return g_desktop->is_start_menu_open(); }
static void api_set_start_menu_open(int open) { g_desktop->set_start_menu_open(open); }

static Desktop_Interface g_desktop_interface = {
	.get_window_count = api_get_window_count,
	.get_window_title = api_get_window_title,
	.is_window_active = api_is_window_active,
	.is_window_visible = api_is_window_visible,
	.is_window_minimized = api_is_window_minimized,
	.toggle_window = api_toggle_window,
	.launch_app = api_launch_app,
	.get_mouse_x = api_get_mouse_x,
	.get_mouse_y = api_get_mouse_y,
	.is_start_menu_open = api_is_start_menu_open,
	.set_start_menu_open = api_set_start_menu_open
};

void Desktop::load_and_run_app(const char* identifier) {
	const char* rbx_path = identifier;
	if (str_equals(identifier, "System Monitor") || str_equals(identifier, "Monitor")) rbx_path = "/sys/sysmon.rbx";
	else if (str_equals(identifier, "Calculator") || str_equals(identifier, "Calc")) rbx_path = "/sys/calculator.rbx";
	else if (str_equals(identifier, "Text Editor") || str_equals(identifier, "Editor")) rbx_path = "/sys/texteditor.rbx";
	else if (str_equals(identifier, "File Explorer") || str_equals(identifier, "Files")) rbx_path = "/sys/filemanager.rbx";
	else if (str_equals(identifier, "Clock")) rbx_path = "/sys/clock.rbx";
	else if (str_equals(identifier, "Calendar") || str_equals(identifier, "Calen")) rbx_path = "/sys/calendar.rbx";
	else if (str_equals(identifier, "Settings") || str_equals(identifier, "Config")) rbx_path = "/sys/settings.rbx";
	else if (str_equals(identifier, "Terminal") || str_equals(identifier, "Term")) rbx_path = "/sys/terminal.rbx";

	for (int i = 0; i < m_window_count; i++) {
		RbxWindow* rbx_win = static_cast<RbxWindow*>(m_windows[i]);
		if (rbx_win && str_equals(rbx_win->get_rbx_path(), rbx_path)) {
			rbx_win->set_visible(true);
			rbx_win->set_minimized(false);
			focus_window(rbx_win);
			draw();
			draw_cursor();
			return;
		}
	}

	void* entry = g_api->load_rbx(rbx_path);
	if (entry) {
		rbx_init_t init_fn = (rbx_init_t)entry;
		rbx_module* mod = init_fn(g_api, &g_desktop_interface);
		if (mod && mod->type == RBX_MODULE_APP_WINDOW) {
			RbxWindow* rbx_win = new RbxWindow(mod, rbx_path);
			add_window(rbx_win);
			focus_window(rbx_win);
			draw();
			draw_cursor();
		}
	}
}

// Global hook registry for launching applications
static void desktop_launch_handler(const char* title) {
	if (g_desktop) {
		g_desktop->load_and_run_app(title);
	}
}

// Entry point of the dynamic desktop binary
extern "C" void _start(const RitOS_API* api) {
	g_api = api;

	// Enable double buffering in the system
	api->enable_double_buffer(1);

	// Register global app launch callback
	// In desktop.rbx we register our launch handler to update windows list
	rit::System::set_launch_app_handler(desktop_launch_handler);

	g_desktop = new Desktop();
	g_desktop->init_modules(&g_desktop_interface);

	// Initial file explorer auto-start
	// g_desktop->load_and_run_app("/sys/filemanager.rbx");
	// g_desktop->load_and_run_app("/sys/terminal.rbx");

	g_desktop->draw();
	g_desktop->draw_cursor();
	api->flush();

	int prev_mx = 40;
	int prev_my = 12;
	uint8_t prev_buttons = 0;
	
	int h = 0, m = 0, s = 0;
	api->get_time(&h, &m, &s);
	int prev_second = s;

	while (1) {
		int mx = prev_mx;
		int my = prev_my;
		uint8_t buttons = prev_buttons;

		// 1. Poll Mouse
		if (api->poll_mouse(&mx, &my, &buttons)) {
			if (mx != prev_mx || my != prev_my || buttons != prev_buttons) {
				g_desktop->update_mouse(mx, my, buttons);
				g_desktop->draw();
				g_desktop->draw_cursor();
				api->flush();
				
				prev_mx = mx;
				prev_my = my;
				prev_buttons = buttons;
			}
		}

		// 2. Poll Keyboard
		char key = 0;
		if (api->poll_keyboard(&key)) {
			if (key == '\t') {
				g_desktop->cycle_focus();
				g_desktop->draw();
				g_desktop->draw_cursor();
				api->flush();
			} else if (key == 27) { // Escape
				ritos::Window* active_win = g_desktop->get_active_window();
				if (active_win) {
					active_win->set_visible(false);
					g_desktop->draw();
					g_desktop->draw_cursor();
					api->flush();
				}
			} else {
				ritos::Window* active_win = g_desktop->get_active_window();
				if (active_win) {
					active_win->handle_key(key);
					g_desktop->draw();
					g_desktop->draw_cursor();
					api->flush();
				}
			}
		}

		// 3. Periodic Clock tick
		int cur_h = 0, cur_m = 0, cur_s = 0;
		api->get_time(&cur_h, &cur_m, &cur_s);
		if (cur_s != prev_second) {
			g_desktop->draw();
			g_desktop->draw_cursor();
			api->flush();
			prev_second = cur_s;
		}

		// Throttle event polling rate
		for (volatile uint32_t i = 0; i < 40000; i++) {
			__asm__ volatile("nop");
		}
	}
}

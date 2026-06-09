#include "../include/ritos/desktop.hpp"
#include "../../framework/include/rit/image.hpp"
#include "../include/ritos/apps.hpp"

namespace ritos {

// Helper: compare two strings
static bool str_equals(const char* s1, const char* s2) {
	int i = 0;
	while (s1[i] != '\0' && s2[i] != '\0') {
		if (s1[i] != s2[i]) return false;
		i++;
	}
	return s1[i] == s2[i];
}

// Helper: Format integer to buffer
static void int_to_str(int n, char* buf) {
	int i = 0;
	bool is_neg = false;
	if (n < 0) {
		is_neg = true;
		n = -n;
	}
	if (n == 0) {
		buf[i++] = '0';
	} else {
		while (n > 0) {
			buf[i++] = (n % 10) + '0';
			n /= 10;
		}
	}
	if (is_neg) {
		buf[i++] = '-';
	}
	buf[i] = '\0';
	// Reverse buffer
	int len = i;
	for (int j = 0; j < len / 2; j++) {
		char temp = buf[j];
		buf[j] = buf[len - j - 1];
		buf[len - j - 1] = temp;
	}
}

Desktop::Desktop()
	: m_bg_color(rit::Color::Blue), m_bar_color(rit::Color::LightGrey), m_bg_pattern('G'), m_window_count(0),
	  m_mouse_x(40), m_mouse_y(12), m_mouse_buttons(0), m_dragged_window(nullptr),
	  m_drag_offset_x(0), m_drag_offset_y(0), m_focus_index(0), m_start_menu_open(false) {
	for (int i = 0; i < 10; i++) {
		m_windows[i] = nullptr;
	}
}

Desktop::~Desktop() {}

void Desktop::add_window(Window* win) {
	if (m_window_count < 10) {
		m_windows[m_window_count++] = win;
	}
}

void Desktop::update_mouse(int x, int y, uint8_t buttons) {
	bool lbutton_was_pressed = (m_mouse_buttons & 1) != 0;
	bool lbutton_is_pressed = (buttons & 1) != 0;
	bool rbutton_was_pressed = (m_mouse_buttons & 2) != 0;
	bool rbutton_is_pressed = (buttons & 2) != 0;

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

	if (!rbutton_was_pressed && rbutton_is_pressed) {
		handle_right_mouse_down(x, y);
	}
}

void Desktop::handle_mouse_down(int x, int y) {
	// 1. If start menu is open, handle clicks
	if (m_start_menu_open) {
		m_start_menu_open = false; // Close by default
		if (x >= 1 && x <= 19 && y >= 14 && y <= 22) {
			int idx = y - 14;
			const char* app_titles[] = {
				"System Monitor",
				"Calculator",
				"Text Editor",
				"File Explorer",
				"Clock",
				"Calendar",
				"Settings"
			};
			if (idx >= 0 && idx < 7) {
				// Launch/focus window
				for (int k = 0; k < m_window_count; k++) {
					if (m_windows[k] != nullptr && str_equals(m_windows[k]->get_title(), app_titles[idx])) {
						m_windows[k]->set_visible(true);
						m_windows[k]->set_minimized(false);
						
						// Bring to front
						Window* clicked = m_windows[k];
						for (int s = k; s < m_window_count - 1; s++) {
							m_windows[s] = m_windows[s + 1];
						}
						m_windows[m_window_count - 1] = clicked;
						
						// Focus
						for (int s = 0; s < m_window_count; s++) {
							if (m_windows[s] != nullptr) {
								m_windows[s]->set_active(s == m_window_count - 1);
							}
						}
						
						// If opening File Explorer, refresh files list
						if (idx == 3) {
							static_cast<FileExplorerWindow*>(m_windows[m_window_count - 1])->refresh_files();
						}
						break;
					}
				}
			} else if (idx == 8) { // Shut down
				rit::System::shutdown();
			}
			draw();
			return;
		}
	}

	// 2. Click on Bottom Taskbar (y == 24)
	if (y == 24) {
		// Start Button click (x: 1-11)
		if (x >= 1 && x <= 11) {
			m_start_menu_open = !m_start_menu_open;
			draw();
			return;
		}

		// App Buttons clicks
		int clicked_app = -1;
		if (x >= 12 && x <= 19) clicked_app = 0;      // Monitor
		else if (x >= 21 && x <= 26) clicked_app = 1; // Calc
		else if (x >= 28 && x <= 35) clicked_app = 2; // Editor
		else if (x >= 37 && x <= 43) clicked_app = 3; // Files
		else if (x >= 45 && x <= 51) clicked_app = 4; // Clock
		else if (x >= 53 && x <= 59) clicked_app = 5; // Calen
		else if (x >= 61 && x <= 68) clicked_app = 6; // Config
		
		if (clicked_app >= 0) {
			const char* app_titles[] = {
				"System Monitor",
				"Calculator",
				"Text Editor",
				"File Explorer",
				"Clock",
				"Calendar",
				"Settings"
			};
			
			// Find window
			for (int k = 0; k < m_window_count; k++) {
				if (m_windows[k] != nullptr && str_equals(m_windows[k]->get_title(), app_titles[clicked_app])) {
					Window* win = m_windows[k];
					if (!win->is_visible() || win->is_minimized()) {
						// Open/Restore it
						win->set_visible(true);
						win->set_minimized(false);
						
						// Bring to front
						for (int s = k; s < m_window_count - 1; s++) {
							m_windows[s] = m_windows[s + 1];
						}
						m_windows[m_window_count - 1] = win;
						
						// Focus
						for (int s = 0; s < m_window_count; s++) {
							if (m_windows[s] != nullptr) {
								m_windows[s]->set_active(s == m_window_count - 1);
							}
						}
						
						// Refresh files list if File Explorer
						if (clicked_app == 3) {
							static_cast<FileExplorerWindow*>(win)->refresh_files();
						}
					} else {
						// Toggle minimize if already topmost and active
						if (win->is_active()) {
							win->set_minimized(true);
						} else {
							// Just bring to front and focus
							for (int s = k; s < m_window_count - 1; s++) {
								m_windows[s] = m_windows[s + 1];
							}
							m_windows[m_window_count - 1] = win;
							
							for (int s = 0; s < m_window_count; s++) {
								if (m_windows[s] != nullptr) {
									m_windows[s]->set_active(s == m_window_count - 1);
								}
							}
						}
					}
					break;
				}
			}
			draw();
		}
		return;
	}

	// 3. Click inside windows
	m_dragged_window = nullptr;
	for (int i = m_window_count - 1; i >= 0; i--) {
		Window* win = m_windows[i];
		if (win != nullptr && win->is_visible() && !win->is_minimized()) {
			int wx = win->get_x();
			int wy = win->get_y();
			int ww = win->get_width();
			int wh = win->get_height();
			
			if (x >= wx && x < wx + ww && y >= wy && y < wy + wh) {
				// Click was inside the window! Focus and bring to front
				if (i < m_window_count - 1) {
					for (int k = i; k < m_window_count - 1; k++) {
						m_windows[k] = m_windows[k + 1];
					}
					m_windows[m_window_count - 1] = win;
				}
				
				// Deactivate others, activate this
				for (int k = 0; k < m_window_count; k++) {
					if (m_windows[k] != nullptr) {
						m_windows[k]->set_active(k == m_window_count - 1);
					}
				}
				
				// Check header click vs body click
				if (y == wy) {
					// Header click! Close button [X]
					if (x >= wx + ww - 3 && x <= wx + ww - 2) {
						win->set_visible(false);
					}
					// Minimize button [-]
					else if (x >= wx + ww - 6 && x <= wx + ww - 5) {
						win->set_minimized(true);
					}
					// Start dragging
					else {
						m_dragged_window = win;
						m_drag_offset_x = x - wx;
						m_drag_offset_y = y - wy;
					}
				} else {
					// Body click! Let window handle click
					win->handle_click(x, y);
				}
				
				draw();
				break;
			}
		}
	}

	// 4. Check Desktop Icons clicks
	// Column 1 (x: 1-8)
	if (x >= 1 && x <= 8) {
		int clicked_icon = -1;
		if (y >= 2 && y <= 5) clicked_icon = 0;      // Monitor
		else if (y >= 7 && y <= 10) clicked_icon = 1; // Calc
		else if (y >= 12 && y <= 15) clicked_icon = 2; // Editor
		else if (y >= 17 && y <= 20) clicked_icon = 3; // Files
		
		if (clicked_icon >= 0) {
			const char* app_titles[] = {
				"System Monitor",
				"Calculator",
				"Text Editor",
				"File Explorer"
			};
			for (int k = 0; k < m_window_count; k++) {
				if (m_windows[k] != nullptr && str_equals(m_windows[k]->get_title(), app_titles[clicked_icon])) {
					m_windows[k]->set_visible(true);
					m_windows[k]->set_minimized(false);
					
					// Bring to front
					Window* clicked = m_windows[k];
					for (int s = k; s < m_window_count - 1; s++) {
						m_windows[s] = m_windows[s + 1];
					}
					m_windows[m_window_count - 1] = clicked;
					
					// Focus
					for (int s = 0; s < m_window_count; s++) {
						if (m_windows[s] != nullptr) {
							m_windows[s]->set_active(s == m_window_count - 1);
						}
					}

					if (clicked_icon == 3) {
						static_cast<FileExplorerWindow*>(m_windows[m_window_count - 1])->refresh_files();
					}
					break;
				}
			}
			draw();
		}
	}
	// Column 2 (x: 9-16)
	else if (x >= 9 && x <= 16) {
		int clicked_icon = -1;
		if (y >= 2 && y <= 5) clicked_icon = 4;      // Clock
		else if (y >= 7 && y <= 10) clicked_icon = 5; // Calen
		else if (y >= 12 && y <= 15) clicked_icon = 6; // Config
		
		if (clicked_icon >= 4) {
			const char* app_titles[] = {
				"System Monitor",
				"Calculator",
				"Text Editor",
				"File Explorer",
				"Clock",
				"Calendar",
				"Settings"
			};
			for (int k = 0; k < m_window_count; k++) {
				if (m_windows[k] != nullptr && str_equals(m_windows[k]->get_title(), app_titles[clicked_icon])) {
					m_windows[k]->set_visible(true);
					m_windows[k]->set_minimized(false);
					
					// Bring to front
					Window* clicked = m_windows[k];
					for (int s = k; s < m_window_count - 1; s++) {
						m_windows[s] = m_windows[s + 1];
					}
					m_windows[m_window_count - 1] = clicked;
					
					// Focus
					for (int s = 0; s < m_window_count; s++) {
						if (m_windows[s] != nullptr) {
							m_windows[s]->set_active(s == m_window_count - 1);
						}
					}
					break;
				}
			}
			draw();
		}
	}
}

void Desktop::handle_right_mouse_down(int x, int y) {
	if (m_start_menu_open) {
		m_start_menu_open = false;
		draw();
		return;
	}
	
	for (int i = m_window_count - 1; i >= 0; i--) {
		Window* win = m_windows[i];
		if (win != nullptr && win->is_visible() && !win->is_minimized()) {
			int wx = win->get_x();
			int wy = win->get_y();
			int ww = win->get_width();
			int wh = win->get_height();
			
			if (x >= wx && x < wx + ww && y >= wy && y < wy + wh) {
				// Route right-click to the window
				win->handle_right_click(x, y);
				draw();
				break;
			}
		}
	}
}

void Desktop::handle_mouse_move(int x, int y) {
	if (m_dragged_window != nullptr) {
		int new_x = x - m_drag_offset_x;
		int new_y = y - m_drag_offset_y;

		// Clamp window bounds to keep it visible on the desktop screen area
		if (new_x < -10) new_x = -10;
		if (new_x > 70) new_x = 70;
		if (new_y < 1) new_y = 1;
		if (new_y > 23) new_y = 23;

		m_dragged_window->set_position(new_x, new_y);
		draw();
	}
}

void Desktop::handle_mouse_up() {
	m_dragged_window = nullptr;
}

void Desktop::draw_cursor() {
	uint16_t* video_memory = (uint16_t*)0xB8000;
	
	// Draw shadow first
	int shadow_offsets[3][2] = {
		{1, 0}, {0, 1}, {1, 1}
	};
	for (int i = 0; i < 3; i++) {
		int sx = m_mouse_x + shadow_offsets[i][0];
		int sy = m_mouse_y + shadow_offsets[i][1];
		if (sx >= 0 && sx < 80 && sy >= 0 && sy < 25) {
			int idx = sy * 80 + sx;
			uint16_t cell = video_memory[idx];
			uint8_t character = cell & 0xFF;
			uint8_t attribute = (cell >> 8) & 0xFF;
			uint8_t fg = attribute & 0x0F;
			// DarkGrey (8) background shadow
			uint8_t new_attribute = fg | (8 << 4);
			video_memory[idx] = (uint16_t)character | ((uint16_t)new_attribute << 8);
		}
	}

	// Draw pointer
	int index = m_mouse_y * 80 + m_mouse_x;
	if (index >= 0 && index < 80 * 25) {
		uint8_t character = 0x1E; // ▲ pointer symbol
		uint8_t attribute = 0x0F | (11 << 4); // White foreground, LightCyan background
		video_memory[index] = (uint16_t)character | ((uint16_t)attribute << 8);
	}
}

void Desktop::draw() {
	// 1. Draw background pattern
	for (int y = 1; y < 24; y++) {
		for (int x = 0; x < 80; x++) {
			if (m_bg_pattern == 'G') {
				if ((x % 4 == 0) && (y % 2 == 0)) {
					rit::System::draw_char('.', rit::Color::LightBlue, m_bg_color, x, y);
				} else {
					rit::System::draw_char(' ', m_bg_color, m_bg_color, x, y);
				}
			} else if (m_bg_pattern == '*') {
				if ((x * 7 + y * 13) % 43 == 0) {
					rit::System::draw_char('*', rit::Color::White, m_bg_color, x, y);
				} else {
					rit::System::draw_char(' ', m_bg_color, m_bg_color, x, y);
				}
			} else { // Solid
				rit::System::draw_char(' ', m_bg_color, m_bg_color, x, y);
			}
		}
	}

	// 1.5 Draw Desktop Icons (using rit::Image)
	// Column 1 (x = 2)
	// Icon 0: Monitor
	{
		const char* monitor_data = 
			"\xDA\xC4\xC4\xBF"
			"\xB3\xDB\xDB\xB3"
			"\xC0\xC1\xC1\xD9";
		uint8_t monitor_colors[] = {
			0x08, 0x08, 0x08, 0x08,
			0x08, 0x0B, 0x0B, 0x08,
			0x08, 0x08, 0x08, 0x08
		};
		rit::Image icon(4, 3, monitor_data, monitor_colors);
		icon.draw(2, 2);
		const char* label = "Monitor";
		for (int i = 0; label[i] != '\0'; i++) {
			rit::System::draw_char(label[i], rit::Color::White, m_bg_color, 1 + i, 5);
		}
	}

	// Icon 1: Calculator
	{
		const char* calc_data = 
			"\xDA\xC4\xC4\xBF"
			"\xB3\xF1\xF7\xB3"
			"\xC0\xC1\xC1\xD9";
		uint8_t calc_colors[] = {
			0x08, 0x08, 0x08, 0x08,
			0x08, 0x0E, 0x0E, 0x08,
			0x08, 0x08, 0x08, 0x08
		};
		rit::Image icon(4, 3, calc_data, calc_colors);
		icon.draw(2, 7);
		const char* label = " Calc  ";
		for (int i = 0; label[i] != '\0'; i++) {
			rit::System::draw_char(label[i], rit::Color::White, m_bg_color, 1 + i, 10);
		}
	}

	// Icon 2: Text Editor
	{
		const char* edit_data = 
			"\xDA\xC4\xC4\xBF"
			"\xB3\x0E\x0E\xB3"
			"\xC0\xC1\xC1\xD9";
		uint8_t edit_colors[] = {
			0x08, 0x08, 0x08, 0x08,
			0x08, 0x0A, 0x0A, 0x08,
			0x08, 0x08, 0x08, 0x08
		};
		rit::Image icon(4, 3, edit_data, edit_colors);
		icon.draw(2, 12);
		const char* label = "Editor ";
		for (int i = 0; label[i] != '\0'; i++) {
			rit::System::draw_char(label[i], rit::Color::White, m_bg_color, 1 + i, 15);
		}
	}

	// Icon 3: File Explorer
	{
		const char* files_data = 
			"\xDA\xC4\xC4\xBF"
			"\xB3\x0E\x0E\xB3" // folder representation
			"\xC0\xC1\xC1\xD9";
		uint8_t files_colors[] = {
			0x08, 0x08, 0x08, 0x08,
			0x08, 0x0E, 0x0E, 0x08,
			0x08, 0x08, 0x08, 0x08
		};
		rit::Image icon(4, 3, files_data, files_colors);
		icon.draw(2, 17);
		const char* label = "Files  ";
		for (int i = 0; label[i] != '\0'; i++) {
			rit::System::draw_char(label[i], rit::Color::White, m_bg_color, 1 + i, 20);
		}
	}

	// Column 2 (x = 10)
	// Icon 4: Clock
	{
		const char* clock_data = 
			"\xDA\xC4\xC4\xBF"
			"\xB3\x18\x1A\xB3" // hands representation
			"\xC0\xC1\xC1\xD9";
		uint8_t clock_colors[] = {
			0x08, 0x08, 0x08, 0x08,
			0x08, 0x0A, 0x0A, 0x08,
			0x08, 0x08, 0x08, 0x08
		};
		rit::Image icon(4, 3, clock_data, clock_colors);
		icon.draw(10, 2);
		const char* label = "Clock  ";
		for (int i = 0; label[i] != '\0'; i++) {
			rit::System::draw_char(label[i], rit::Color::White, m_bg_color, 9 + i, 5);
		}
	}

	// Icon 5: Calendar
	{
		const char* cal_data = 
			"\xDA\xC4\xC4\xBF"
			"\xB3\x33\x31\xB3" // '31' representation
			"\xC0\xC1\xC1\xD9";
		uint8_t cal_colors[] = {
			0x08, 0x08, 0x08, 0x08,
			0x08, 0x0C, 0x0C, 0x08,
			0x08, 0x08, 0x08, 0x08
		};
		rit::Image icon(4, 3, cal_data, cal_colors);
		icon.draw(10, 7);
		const char* label = "Calen  ";
		for (int i = 0; label[i] != '\0'; i++) {
			rit::System::draw_char(label[i], rit::Color::White, m_bg_color, 9 + i, 10);
		}
	}

	// Icon 6: Settings
	{
		const char* set_data = 
			"\xDA\xC4\xC4\xBF"
			"\xB3\x0F\x0F\xB3"
			"\xC0\xC1\xC1\xD9";
		uint8_t set_colors[] = {
			0x08, 0x08, 0x08, 0x08,
			0x08, 0x0D, 0x0D, 0x08,
			0x08, 0x08, 0x08, 0x08
		};
		rit::Image icon(4, 3, set_data, set_colors);
		icon.draw(10, 12);
		const char* label = "Config ";
		for (int i = 0; label[i] != '\0'; i++) {
			rit::System::draw_char(label[i], rit::Color::White, m_bg_color, 9 + i, 15);
		}
	}

	// 2. Draw Top Status Bar (Row 0)
	for (int x = 0; x < 80; x++) {
		rit::System::draw_char(' ', rit::Color::White, rit::Color::DarkGrey, x, 0);
	}
	const char* status_shortcuts = " [Tab] Cycle Focus  |  [Esc] Close Active";
	for (int i = 0; status_shortcuts[i] != '\0'; i++) {
		rit::System::draw_char(status_shortcuts[i], rit::Color::LightGrey, rit::Color::DarkGrey, 2 + i, 0);
	}

	// Active Window indicator on Top Status Bar
	Window* active_win = get_active_window();
	const char* active_prefix = "Active: ";
	int act_x = 44;
	for (int i = 0; active_prefix[i] != '\0'; i++) {
		rit::System::draw_char(active_prefix[i], rit::Color::LightCyan, rit::Color::DarkGrey, act_x + i, 0);
	}
	act_x += 8;
	if (active_win != nullptr) {
		const char* title = active_win->get_title();
		for (int i = 0; title[i] != '\0'; i++) {
			rit::System::draw_char(title[i], rit::Color::White, rit::Color::DarkGrey, act_x + i, 0);
		}
	} else {
		const char* none_txt = "None";
		for (int i = 0; none_txt[i] != '\0'; i++) {
			rit::System::draw_char(none_txt[i], rit::Color::LightGrey, rit::Color::DarkGrey, act_x + i, 0);
		}
	}

	// Heap Memory indicator in top status bar (right-aligned)
	size_t heap_usage = rit::System::get_heap_usage();
	char heap_buf[16];
	int_to_str(heap_usage, heap_buf);
	int hp_x = 78;
	rit::System::draw_char('B', rit::Color::LightGrey, rit::Color::DarkGrey, hp_x--, 0);
	rit::System::draw_char(' ', rit::Color::LightGrey, rit::Color::DarkGrey, hp_x--, 0);
	int h_idx = 0;
	while (heap_buf[h_idx] != '\0') h_idx++;
	for (int i = h_idx - 1; i >= 0; i--) {
		rit::System::draw_char(heap_buf[i], rit::Color::White, rit::Color::DarkGrey, hp_x--, 0);
	}
	const char* mem_lbl = " Mem:";
	for (int i = 4; i >= 0; i--) {
		rit::System::draw_char(mem_lbl[i], rit::Color::LightCyan, rit::Color::DarkGrey, hp_x--, 0);
	}

	// 3. Draw Windows
	for (int i = 0; i < m_window_count; i++) {
		if (m_windows[i] != nullptr) {
			m_windows[i]->draw();
		}
	}

	// 4. Draw Bottom Taskbar (Row 24)
	for (int x = 0; x < 80; x++) {
		rit::System::draw_char(' ', rit::Color::Black, m_bar_color, x, 24);
	}

	// Start button
	rit::Color start_bg = m_start_menu_open ? rit::Color::Blue : m_bar_color;
	rit::Color start_fg = m_start_menu_open ? rit::Color::White : rit::Color::Black;
	if (!m_start_menu_open && m_mouse_y == 24 && m_mouse_x >= 1 && m_mouse_x <= 11) {
		start_bg = rit::Color::LightCyan;
	}
	const char* start_txt = " \x0F Start   ";
	for (int i = 0; start_txt[i] != '\0'; i++) {
		rit::System::draw_char(start_txt[i], start_fg, start_bg, 1 + i, 24);
	}

	// Taskbar applications
	const char* dock_labels[] = {
		"SysMon", "Calc", "Editor", "Files", "Clock", "Calen", "Config"
	};
	int dock_xs[] = { 12, 21, 28, 37, 45, 53, 61 };
	int dock_widths[] = { 8, 6, 8, 7, 7, 7, 8 };
	const char* dock_names[] = {
		"System Monitor",
		"Calculator",
		"Text Editor",
		"File Explorer",
		"Clock",
		"Calendar",
		"Settings"
	};

	for (int i = 0; i < 7; i++) {
		int bx = dock_xs[i];
		int bw = dock_widths[i];
		
		Window* win = nullptr;
		for (int k = 0; k < m_window_count; k++) {
			if (m_windows[k] != nullptr && str_equals(m_windows[k]->get_title(), dock_names[i])) {
				win = m_windows[k];
				break;
			}
		}
		
		bool is_open = (win != nullptr && win->is_visible() && !win->is_minimized());
		bool is_active_win = (win != nullptr && win->is_visible() && !win->is_minimized() && win->is_active());
		bool is_hovered = (m_mouse_y == 24 && m_mouse_x >= bx && m_mouse_x < bx + bw);
		
		rit::Color fg = rit::Color::Black;
		rit::Color bg = m_bar_color;
		
		if (is_hovered) {
			bg = rit::Color::LightCyan;
		} else if (is_active_win) {
			bg = rit::Color::Blue;
			fg = rit::Color::White;
		} else if (is_open) {
			bg = rit::Color::DarkGrey;
			fg = rit::Color::White;
		}
		
		rit::System::draw_char('[', fg, bg, bx, 24);
		int char_idx = 0;
		while (dock_labels[i][char_idx] != '\0') {
			rit::System::draw_char(dock_labels[i][char_idx], fg, bg, bx + 1 + char_idx, 24);
			char_idx++;
		}
		
		if (is_open) {
			rit::System::draw_char('*', rit::Color::LightGreen, bg, bx + 1 + char_idx, 24);
			rit::System::draw_char(']', fg, bg, bx + 2 + char_idx, 24);
		} else {
			rit::System::draw_char(']', fg, bg, bx + 1 + char_idx, 24);
		}
	}

	// Draw digital clock in taskbar
	int h = 0, m = 0, s = 0;
	rit::System::get_time(h, m, s);
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
	for (int i = 0; clock_buf[i] != '\0'; i++) {
		rit::System::draw_char(clock_buf[i], rit::Color::Black, m_bar_color, 71 + i, 24);
	}

	// 5. Draw Start Menu Popup on top of everything (pops up from bottom-left)
	if (m_start_menu_open) {
		int sm_x = 1;
		int sm_y = 13;
		int sm_w = 19;
		int sm_h = 11;
		
		rit::System::draw_char('\xDA', rit::Color::White, rit::Color::DarkGrey, sm_x, sm_y);
		for (int col = 1; col < sm_w - 1; col++) {
			rit::System::draw_char('\xC4', rit::Color::White, rit::Color::DarkGrey, sm_x + col, sm_y);
		}
		rit::System::draw_char('\xBF', rit::Color::White, rit::Color::DarkGrey, sm_x + sm_w - 1, sm_y);
		
		for (int row = 1; row < sm_h - 1; row++) {
			rit::System::draw_char('\xB3', rit::Color::White, rit::Color::DarkGrey, sm_x, sm_y + row);
			for (int col = 1; col < sm_w - 1; col++) {
				rit::System::draw_char(' ', rit::Color::White, rit::Color::DarkGrey, sm_x + col, sm_y + row);
			}
			rit::System::draw_char('\xB3', rit::Color::White, rit::Color::DarkGrey, sm_x + sm_w - 1, sm_y + row);
		}
		
		rit::System::draw_char('\xC0', rit::Color::White, rit::Color::DarkGrey, sm_x, sm_y + sm_h - 1);
		for (int col = 1; col < sm_w - 1; col++) {
			rit::System::draw_char('\xC4', rit::Color::White, rit::Color::DarkGrey, sm_x + col, sm_y + sm_h - 1);
		}
		rit::System::draw_char('\xD9', rit::Color::White, rit::Color::DarkGrey, sm_x + sm_w - 1, sm_y + sm_h - 1);
		
		const char* items[] = {
			"  Monitor",
			"  Calculator",
			"  Text Editor",
			"  File Explorer",
			"  Clock",
			"  Calendar",
			"  Settings",
			" \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4",
			"  Shut Down"
		};
		
		for (int i = 0; i < 9; i++) {
			int item_y = sm_y + 1 + i;
			bool is_hovered = (m_mouse_x >= sm_x + 1 && m_mouse_x < sm_x + sm_w - 1 && m_mouse_y == item_y);
			if (i == 7) is_hovered = false;
			
			rit::Color bg = rit::Color::DarkGrey;
			rit::Color fg = rit::Color::White;
			
			if (is_hovered) {
				bg = rit::Color::Blue;
			}
			
			int col = 0;
			while (items[i][col] != '\0') {
				rit::System::draw_char(items[i][col], fg, bg, sm_x + 1 + col, item_y);
				col++;
			}
			
			for (int x = col; x < sm_w - 2; x++) {
				rit::System::draw_char(' ', fg, bg, sm_x + 1 + x, item_y);
			}
		}
	}
}

void Desktop::cycle_focus() {
	m_focus_index++;
	if (m_focus_index > 2) {
		m_focus_index = 1;
	}
	draw();
	draw_cursor();
}

void Desktop::trigger_focused_action() {
	if (m_focus_index == 1) {
		rit::System::shutdown();
	} else if (m_focus_index == 2) {
		rit::System::reboot();
	}
}

Window* Desktop::get_active_window() {
	for (int i = m_window_count - 1; i >= 0; i--) {
		if (m_windows[i] != nullptr && m_windows[i]->is_visible() && !m_windows[i]->is_minimized() && m_windows[i]->is_active()) {
			return m_windows[i];
		}
	}
	return nullptr;
}

} // namespace ritos

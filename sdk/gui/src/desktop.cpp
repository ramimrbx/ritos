#include "../include/ritos/desktop.hpp"
#include "../../framework/include/rit/image.hpp"
#include "../include/ritos/apps.hpp"

namespace ritos {

static bool str_equals(const char* s1, const char* s2) {
	int i = 0;
	while (s1[i] != '\0' && s2[i] != '\0') {
		if (s1[i] != s2[i]) return false;
		i++;
	}
	return s1[i] == s2[i];
}

static void int_to_str(int n, char* buf) {
	int i = 0;
	bool is_neg = false;
	if (n < 0) { is_neg = true; n = -n; }
	if (n == 0) {
		buf[i++] = '0';
	} else {
		while (n > 0) { buf[i++] = (n % 10) + '0'; n /= 10; }
	}
	if (is_neg) buf[i++] = '-';
	buf[i] = '\0';
	int len = i;
	for (int j = 0; j < len / 2; j++) {
		char tmp = buf[j]; buf[j] = buf[len - j - 1]; buf[len - j - 1] = tmp;
	}
}

// Helper: draw a null-terminated string at (x,y) with given colors
static void draw_str(const char* s, rit::Color fg, rit::Color bg, int x, int y) {
	for (int i = 0; s[i] != '\0'; i++) {
		rit::System::draw_char(s[i], fg, bg, x + i, y);
	}
}

Desktop::Desktop()
	: m_bg_color(rit::Color::Black), m_bar_color(rit::Color::Blue),
	  m_bg_pattern('G'), m_window_count(0),
	  m_mouse_x(40), m_mouse_y(12), m_mouse_buttons(0),
	  m_dragged_window(nullptr), m_drag_offset_x(0), m_drag_offset_y(0),
	  m_focus_index(0), m_start_menu_open(false) {
	for (int i = 0; i < 10; i++) m_windows[i] = nullptr;
}

Desktop::~Desktop() {}

void Desktop::add_window(Window* win) {
	if (m_window_count < 10) m_windows[m_window_count++] = win;
}

void Desktop::update_mouse(int x, int y, uint8_t buttons) {
	bool lbtn_was = (m_mouse_buttons & 1) != 0;
	bool lbtn_now = (buttons & 1) != 0;
	bool rbtn_was = (m_mouse_buttons & 2) != 0;
	bool rbtn_now = (buttons & 2) != 0;

	m_mouse_x = x; m_mouse_y = y; m_mouse_buttons = buttons;

	if (!lbtn_was && lbtn_now)       handle_mouse_down(x, y);
	else if (lbtn_was && !lbtn_now)  handle_mouse_up();
	else if (lbtn_now)               handle_mouse_move(x, y);

	if (!rbtn_was && rbtn_now) handle_right_mouse_down(x, y);
}

void Desktop::handle_mouse_down(int x, int y) {
	// Start menu open – route clicks there first
	if (m_start_menu_open) {
		m_start_menu_open = false;
		if (x >= 1 && x <= 19 && y >= 14 && y <= 22) {
			int idx = y - 14;
			const char* app_titles[] = {
				"System Monitor","Calculator","Text Editor",
				"File Explorer","Clock","Calendar","Settings"
			};
			if (idx >= 0 && idx < 7) {
				for (int k = 0; k < m_window_count; k++) {
					if (m_windows[k] && str_equals(m_windows[k]->get_title(), app_titles[idx])) {
						m_windows[k]->set_visible(true);
						m_windows[k]->set_minimized(false);
						Window* w = m_windows[k];
						for (int s = k; s < m_window_count - 1; s++) m_windows[s] = m_windows[s+1];
						m_windows[m_window_count-1] = w;
						for (int s = 0; s < m_window_count; s++)
							if (m_windows[s]) m_windows[s]->set_active(s == m_window_count-1);
						if (idx == 3) static_cast<FileExplorerWindow*>(m_windows[m_window_count-1])->refresh_files();
						break;
					}
				}
			} else if (idx == 8) {
				rit::System::shutdown();
			}
		}
		draw(); return;
	}

	// Bottom taskbar
	if (y == 24) {
		// Start button x:1–11
		if (x >= 1 && x <= 11) {
			m_start_menu_open = !m_start_menu_open;
			draw(); return;
		}
		// App buttons (same x-ranges as original)
		int clicked_app = -1;
		if      (x >= 12 && x <= 19) clicked_app = 0;
		else if (x >= 21 && x <= 26) clicked_app = 1;
		else if (x >= 28 && x <= 35) clicked_app = 2;
		else if (x >= 37 && x <= 43) clicked_app = 3;
		else if (x >= 45 && x <= 51) clicked_app = 4;
		else if (x >= 53 && x <= 59) clicked_app = 5;
		else if (x >= 61 && x <= 68) clicked_app = 6;

		if (clicked_app >= 0) {
			const char* app_titles[] = {
				"System Monitor","Calculator","Text Editor",
				"File Explorer","Clock","Calendar","Settings"
			};
			for (int k = 0; k < m_window_count; k++) {
				if (m_windows[k] && str_equals(m_windows[k]->get_title(), app_titles[clicked_app])) {
					Window* win = m_windows[k];
					if (!win->is_visible() || win->is_minimized()) {
						win->set_visible(true); win->set_minimized(false);
						for (int s = k; s < m_window_count-1; s++) m_windows[s] = m_windows[s+1];
						m_windows[m_window_count-1] = win;
						for (int s = 0; s < m_window_count; s++)
							if (m_windows[s]) m_windows[s]->set_active(s == m_window_count-1);
						if (clicked_app == 3) static_cast<FileExplorerWindow*>(win)->refresh_files();
					} else {
						if (win->is_active()) {
							win->set_minimized(true);
						} else {
							for (int s = k; s < m_window_count-1; s++) m_windows[s] = m_windows[s+1];
							m_windows[m_window_count-1] = win;
							for (int s = 0; s < m_window_count; s++)
								if (m_windows[s]) m_windows[s]->set_active(s == m_window_count-1);
						}
					}
					break;
				}
			}
			draw();
		}
		return;
	}

	// Window clicks
	m_dragged_window = nullptr;
	for (int i = m_window_count - 1; i >= 0; i--) {
		Window* win = m_windows[i];
		if (!win || !win->is_visible() || win->is_minimized()) continue;
		int wx = win->get_x(), wy = win->get_y();
		int ww = win->get_width(), wh = win->get_height();
		if (x >= wx && x < wx+ww && y >= wy && y < wy+wh) {
			if (i < m_window_count-1) {
				for (int k = i; k < m_window_count-1; k++) m_windows[k] = m_windows[k+1];
				m_windows[m_window_count-1] = win;
			}
			for (int k = 0; k < m_window_count; k++)
				if (m_windows[k]) m_windows[k]->set_active(k == m_window_count-1);
			if (y == wy) {
				if (x >= wx+ww-3 && x <= wx+ww-2)      win->set_visible(false);
				else if (x >= wx+ww-6 && x <= wx+ww-5) win->set_minimized(true);
				else {
					m_dragged_window = win;
					m_drag_offset_x = x - wx;
					m_drag_offset_y = y - wy;
				}
			} else {
				win->handle_click(x, y);
			}
			draw(); break;
		}
	}

	// Desktop icon clicks – Column 1 (x:1–8)
	if (x >= 1 && x <= 8) {
		int clicked_icon = -1;
		if      (y >= 2  && y <= 5)  clicked_icon = 0;
		else if (y >= 7  && y <= 10) clicked_icon = 1;
		else if (y >= 12 && y <= 15) clicked_icon = 2;
		else if (y >= 17 && y <= 20) clicked_icon = 3;
		if (clicked_icon >= 0) {
			const char* titles[] = {"System Monitor","Calculator","Text Editor","File Explorer"};
			for (int k = 0; k < m_window_count; k++) {
				if (m_windows[k] && str_equals(m_windows[k]->get_title(), titles[clicked_icon])) {
					m_windows[k]->set_visible(true); m_windows[k]->set_minimized(false);
					Window* w = m_windows[k];
					for (int s = k; s < m_window_count-1; s++) m_windows[s] = m_windows[s+1];
					m_windows[m_window_count-1] = w;
					for (int s = 0; s < m_window_count; s++)
						if (m_windows[s]) m_windows[s]->set_active(s == m_window_count-1);
					if (clicked_icon == 3) static_cast<FileExplorerWindow*>(m_windows[m_window_count-1])->refresh_files();
					break;
				}
			}
			draw();
		}
	}
	// Desktop icon clicks – Column 2 (x:9–16)
	else if (x >= 9 && x <= 16) {
		int clicked_icon = -1;
		if      (y >= 2  && y <= 5)  clicked_icon = 4;
		else if (y >= 7  && y <= 10) clicked_icon = 5;
		else if (y >= 12 && y <= 15) clicked_icon = 6;
		if (clicked_icon >= 4) {
			const char* titles[] = {
				"System Monitor","Calculator","Text Editor","File Explorer","Clock","Calendar","Settings"
			};
			for (int k = 0; k < m_window_count; k++) {
				if (m_windows[k] && str_equals(m_windows[k]->get_title(), titles[clicked_icon])) {
					m_windows[k]->set_visible(true); m_windows[k]->set_minimized(false);
					Window* w = m_windows[k];
					for (int s = k; s < m_window_count-1; s++) m_windows[s] = m_windows[s+1];
					m_windows[m_window_count-1] = w;
					for (int s = 0; s < m_window_count; s++)
						if (m_windows[s]) m_windows[s]->set_active(s == m_window_count-1);
					break;
				}
			}
			draw();
		}
	}
}

void Desktop::handle_right_mouse_down(int x, int y) {
	if (m_start_menu_open) { m_start_menu_open = false; draw(); return; }
	for (int i = m_window_count-1; i >= 0; i--) {
		Window* win = m_windows[i];
		if (!win || !win->is_visible() || win->is_minimized()) continue;
		if (x >= win->get_x() && x < win->get_x()+win->get_width() &&
		    y >= win->get_y() && y < win->get_y()+win->get_height()) {
			win->handle_right_click(x, y); draw(); break;
		}
	}
}

void Desktop::handle_mouse_move(int x, int y) {
	if (m_dragged_window) {
		int nx = x - m_drag_offset_x, ny = y - m_drag_offset_y;
		if (nx < -10) nx = -10; if (nx > 70) nx = 70;
		if (ny < 1)   ny = 1;   if (ny > 23) ny = 23;
		m_dragged_window->set_position(nx, ny);
		draw();
	}
}

void Desktop::handle_mouse_up() { m_dragged_window = nullptr; }

void Desktop::draw_cursor() {
	uint16_t* vm = (uint16_t*)0xB8000;
	// Subtle shadow
	int offsets[3][2] = {{1,0},{0,1},{1,1}};
	for (int i = 0; i < 3; i++) {
		int sx = m_mouse_x + offsets[i][0], sy = m_mouse_y + offsets[i][1];
		if (sx >= 0 && sx < 80 && sy >= 0 && sy < 25) {
			int idx = sy*80+sx;
			uint16_t cell = vm[idx];
			uint8_t  fg   = (cell >> 8) & 0x0F;
			vm[idx] = (uint16_t)(cell & 0xFF) | ((uint16_t)(fg | (0 << 4)) << 8);
		}
	}
	// Cursor glyph: ▲ (0x1E) in LightGreen on Black
	int idx = m_mouse_y * 80 + m_mouse_x;
	if (idx >= 0 && idx < 80*25)
		vm[idx] = (uint16_t)0x1E | ((uint16_t)(0x0A | (0 << 4)) << 8);
}

void Desktop::draw() {
	// ─── 1. Desktop background ───────────────────────────────────────────────
	for (int dy = 1; dy < 24; dy++) {
		for (int dx = 0; dx < 80; dx++) {
			if (m_bg_pattern == 'G') {
				// Subtle dot-grid in DarkGrey on current bg
				if (dx % 6 == 0 && dy % 3 == 0)
					rit::System::draw_char('\xFA', rit::Color::DarkGrey, m_bg_color, dx, dy);
				else
					rit::System::draw_char(' ', m_bg_color, m_bg_color, dx, dy);
			} else if (m_bg_pattern == '*') {
				if ((dx * 7 + dy * 13) % 43 == 0)
					rit::System::draw_char('\xFA', rit::Color::DarkGrey, m_bg_color, dx, dy);
				else
					rit::System::draw_char(' ', m_bg_color, m_bg_color, dx, dy);
			} else {
				rit::System::draw_char(' ', m_bg_color, m_bg_color, dx, dy);
			}
		}
	}

	// ─── 2. Desktop Icons ────────────────────────────────────────────────────
	// All icons: 4×3, LightCyan double border, unique content char per app.
	// Color byte = (bg<<4)|fg; all on Black bg → upper nibble = 0.

	// Icon 0 – System Monitor  (LightGreen ░ bars)
	{
		static const char mon_d[] =
			"\xC9\xCD\xCD\xBB"
			"\xBA\xB0\xB0\xBA"
			"\xC8\xCD\xCD\xBC";
		static const uint8_t mon_c[] = {
			0x0B,0x0B,0x0B,0x0B,
			0x0B,0x0A,0x0A,0x0B,
			0x0B,0x0B,0x0B,0x0B
		};
		rit::Image(4,3,mon_d,mon_c).draw(2,2);
		draw_str("SysMon ", rit::Color::LightGreen, m_bg_color, 1, 5);
	}

	// Icon 1 – Calculator  (LightBrown ± ÷)
	{
		static const char cal_d[] =
			"\xC9\xCD\xCD\xBB"
			"\xBA\xF1\xF6\xBA"
			"\xC8\xCD\xCD\xBC";
		static const uint8_t cal_c[] = {
			0x0B,0x0B,0x0B,0x0B,
			0x0B,0x0E,0x0E,0x0B,
			0x0B,0x0B,0x0B,0x0B
		};
		rit::Image(4,3,cal_d,cal_c).draw(2,7);
		draw_str(" Calc  ", rit::Color::LightBrown, m_bg_color, 1, 10);
	}

	// Icon 2 – Text Editor  (White ≡ lines)
	{
		static const char ed_d[] =
			"\xC9\xCD\xCD\xBB"
			"\xBA\xF0\xF0\xBA"
			"\xC8\xCD\xCD\xBC";
		static const uint8_t ed_c[] = {
			0x0B,0x0B,0x0B,0x0B,
			0x0B,0x0F,0x0F,0x0B,
			0x0B,0x0B,0x0B,0x0B
		};
		rit::Image(4,3,ed_d,ed_c).draw(2,12);
		draw_str("Editor ", rit::Color::White, m_bg_color, 1, 15);
	}

	// Icon 3 – File Explorer  (LightBrown ♦ ♦)
	{
		static const char fe_d[] =
			"\xC9\xCD\xCD\xBB"
			"\xBA\x04\x04\xBA"
			"\xC8\xCD\xCD\xBC";
		static const uint8_t fe_c[] = {
			0x0B,0x0B,0x0B,0x0B,
			0x0B,0x0E,0x0E,0x0B,
			0x0B,0x0B,0x0B,0x0B
		};
		rit::Image(4,3,fe_d,fe_c).draw(2,17);
		draw_str("Files  ", rit::Color::LightBrown, m_bg_color, 1, 20);
	}

	// Icon 4 – Clock  (LightMagenta ○ ·)
	{
		static const char clk_d[] =
			"\xC9\xCD\xCD\xBB"
			"\xBA\x09\xFA\xBA"
			"\xC8\xCD\xCD\xBC";
		static const uint8_t clk_c[] = {
			0x0B,0x0B,0x0B,0x0B,
			0x0B,0x0D,0x0D,0x0B,
			0x0B,0x0B,0x0B,0x0B
		};
		rit::Image(4,3,clk_d,clk_c).draw(10,2);
		draw_str("Clock  ", rit::Color::LightMagenta, m_bg_color, 9, 5);
	}

	// Icon 5 – Calendar  (LightRed 3 1)
	{
		static const char cln_d[] =
			"\xC9\xCD\xCD\xBB"
			"\xBA\x33\x31\xBA"
			"\xC8\xCD\xCD\xBC";
		static const uint8_t cln_c[] = {
			0x0B,0x0B,0x0B,0x0B,
			0x0B,0x0C,0x0C,0x0B,
			0x0B,0x0B,0x0B,0x0B
		};
		rit::Image(4,3,cln_d,cln_c).draw(10,7);
		draw_str("Calen  ", rit::Color::LightRed, m_bg_color, 9, 10);
	}

	// Icon 6 – Settings  (LightMagenta ☼ ☼)
	{
		static const char set_d[] =
			"\xC9\xCD\xCD\xBB"
			"\xBA\x0F\x0F\xBA"
			"\xC8\xCD\xCD\xBC";
		static const uint8_t set_c[] = {
			0x0B,0x0B,0x0B,0x0B,
			0x0B,0x0D,0x0D,0x0B,
			0x0B,0x0B,0x0B,0x0B
		};
		rit::Image(4,3,set_d,set_c).draw(10,12);
		draw_str("Config ", rit::Color::LightMagenta, m_bg_color, 9, 15);
	}

	// ─── 3. Top Status Bar (row 0) ───────────────────────────────────────────
	for (int dx = 0; dx < 80; dx++)
		rit::System::draw_char(' ', rit::Color::LightGrey, rit::Color::DarkGrey, dx, 0);

	// Logo
	rit::System::draw_char('\x0F', rit::Color::LightGreen, rit::Color::DarkGrey, 1, 0);

	// Keyboard shortcuts
	draw_str(" [", rit::Color::LightGrey, rit::Color::DarkGrey, 2, 0);
	draw_str("Tab", rit::Color::LightCyan, rit::Color::DarkGrey, 4, 0);
	draw_str("] Focus  [", rit::Color::LightGrey, rit::Color::DarkGrey, 7, 0);
	draw_str("Esc", rit::Color::LightCyan, rit::Color::DarkGrey, 17, 0);
	draw_str("] Close", rit::Color::LightGrey, rit::Color::DarkGrey, 20, 0);

	// Separator
	rit::System::draw_char('\xB3', rit::Color::DarkGrey, rit::Color::DarkGrey, 27, 0);

	// Active window label
	draw_str(" Active: ", rit::Color::LightCyan, rit::Color::DarkGrey, 28, 0);
	Window* active_win = get_active_window();
	int act_x = 37;
	if (active_win) {
		const char* t = active_win->get_title();
		for (int i = 0; t[i] && act_x + i < 62; i++)
			rit::System::draw_char(t[i], rit::Color::White, rit::Color::DarkGrey, act_x + i, 0);
	} else {
		draw_str("None", rit::Color::DarkGrey, rit::Color::DarkGrey, act_x, 0);
	}

	// Separator
	rit::System::draw_char('\xB3', rit::Color::DarkGrey, rit::Color::DarkGrey, 62, 0);

	// Heap memory (right side)
	size_t heap = rit::System::get_heap_usage();
	char hbuf[16]; int_to_str((int)heap, hbuf);
	draw_str(" Mem:", rit::Color::LightCyan, rit::Color::DarkGrey, 63, 0);
	int hx = 68;
	for (int i = 0; hbuf[i]; i++) rit::System::draw_char(hbuf[i], rit::Color::White, rit::Color::DarkGrey, hx++, 0);
	draw_str("b", rit::Color::LightGrey, rit::Color::DarkGrey, hx, 0);

	// ─── 4. Draw Windows ─────────────────────────────────────────────────────
	for (int i = 0; i < m_window_count; i++)
		if (m_windows[i]) m_windows[i]->draw();

	// ─── 5. Bottom Taskbar (row 24) ───────────────────────────────────────────
	for (int dx = 0; dx < 80; dx++)
		rit::System::draw_char(' ', rit::Color::LightGrey, rit::Color::Blue, dx, 24);

	// Start button (x:1–11)
	rit::Color s_bg = rit::Color::Blue, s_fg = rit::Color::White;
	if (m_start_menu_open) { s_bg = rit::Color::Cyan; s_fg = rit::Color::Black; }
	else if (m_mouse_y == 24 && m_mouse_x >= 1 && m_mouse_x <= 11) { s_bg = rit::Color::Cyan; s_fg = rit::Color::Black; }
	rit::System::draw_char(' ',    s_fg, s_bg, 1, 24);
	rit::System::draw_char('\x0F', rit::Color::LightGreen, s_bg, 2, 24); // ☼ logo
	draw_str(" RitOS  ", s_fg, s_bg, 3, 24);

	// Separator after Start
	rit::System::draw_char('\xB3', rit::Color::DarkGrey, rit::Color::Blue, 12, 24);

	// App dock buttons (same x-positions as original for click compatibility)
	const char* dock_labels[] = { "SysMon","Calc","Editor","Files","Clock","Calen","Config" };
	int dock_xs[]     = { 13, 21, 28, 37, 45, 53, 61 };
	int dock_widths[] = {  7,  6,  8,  7,  7,  7,  8 };
	const char* dock_names[] = {
		"System Monitor","Calculator","Text Editor",
		"File Explorer","Clock","Calendar","Settings"
	};

	for (int i = 0; i < 7; i++) {
		int bx = dock_xs[i], bw = dock_widths[i];

		Window* win = nullptr;
		for (int k = 0; k < m_window_count; k++) {
			if (m_windows[k] && str_equals(m_windows[k]->get_title(), dock_names[i])) { win = m_windows[k]; break; }
		}

		bool is_open    = win && win->is_visible() && !win->is_minimized();
		bool is_active  = is_open && win->is_active();
		bool is_hovered = (m_mouse_y == 24 && m_mouse_x >= bx && m_mouse_x < bx + bw);

		rit::Color fg = rit::Color::LightGrey, bg = rit::Color::Blue;
		if (is_hovered)       { bg = rit::Color::Cyan;    fg = rit::Color::Black; }
		else if (is_active)   { bg = rit::Color::Blue;    fg = rit::Color::White; }
		else if (is_open)     { bg = rit::Color::DarkGrey; fg = rit::Color::LightGrey; }

		rit::System::draw_char('[', fg, bg, bx, 24);
		int ci = 0;
		while (dock_labels[i][ci]) {
			rit::System::draw_char(dock_labels[i][ci], fg, bg, bx + 1 + ci, 24);
			ci++;
		}
		if (is_open) {
			// Small dot indicator
			rit::System::draw_char('\xFA', rit::Color::LightGreen, bg, bx + 1 + ci, 24);
			rit::System::draw_char(']', fg, bg, bx + 2 + ci, 24);
		} else {
			rit::System::draw_char(']', fg, bg, bx + 1 + ci, 24);
		}
	}

	// Separator before clock
	rit::System::draw_char('\xB3', rit::Color::DarkGrey, rit::Color::Blue, 70, 24);

	// Clock (right side, x:71–79)
	int h = 0, m = 0, s = 0; rit::System::get_time(h, m, s);
	char clkbuf[9];
	clkbuf[0]=(h/10)+'0'; clkbuf[1]=(h%10)+'0'; clkbuf[2]=':';
	clkbuf[3]=(m/10)+'0'; clkbuf[4]=(m%10)+'0'; clkbuf[5]=':';
	clkbuf[6]=(s/10)+'0'; clkbuf[7]=(s%10)+'0'; clkbuf[8]='\0';
	draw_str(clkbuf, rit::Color::LightGreen, rit::Color::Blue, 71, 24);

	// ─── 6. Start Menu Popup ──────────────────────────────────────────────────
	// Layout preserves original y-positions for click compatibility:
	//   y=13: top border (title embedded)
	//   y=14–20: app items 0–6  (idx = y-14, matching handle_mouse_down)
	//   y=21: divider            (idx=7, no action)
	//   y=22: Shutdown           (idx=8)
	//   y=23: bottom border
	if (m_start_menu_open) {
		const int sm_x = 1, sm_y = 13, sm_w = 20, sm_h = 11;

		// ── Top border with embedded title ──
		rit::System::draw_char('\xC9', rit::Color::LightCyan, rit::Color::DarkGrey, sm_x, sm_y);
		const char* hdr_title = "\xCD\xCD \x0F RitOS \xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD";
		int hti = 0;
		for (int c = 1; c < sm_w-1; c++) {
			if (hdr_title[hti]) {
				rit::Color fc = (hdr_title[hti]=='\x0F') ? rit::Color::LightGreen : rit::Color::LightCyan;
				rit::System::draw_char(hdr_title[hti++], fc, rit::Color::DarkGrey, sm_x+c, sm_y);
			} else {
				rit::System::draw_char('\xCD', rit::Color::LightCyan, rit::Color::DarkGrey, sm_x+c, sm_y);
			}
		}
		rit::System::draw_char('\xBB', rit::Color::LightCyan, rit::Color::DarkGrey, sm_x+sm_w-1, sm_y);

		// ── Side borders + clear content rows ──
		for (int r = 1; r < sm_h-1; r++) {
			rit::System::draw_char('\xBA', rit::Color::LightCyan, rit::Color::DarkGrey, sm_x, sm_y+r);
			for (int c = 1; c < sm_w-1; c++)
				rit::System::draw_char(' ', rit::Color::White, rit::Color::DarkGrey, sm_x+c, sm_y+r);
			rit::System::draw_char('\xBA', rit::Color::LightCyan, rit::Color::DarkGrey, sm_x+sm_w-1, sm_y+r);
		}

		// ── Bottom border ──
		rit::System::draw_char('\xC8', rit::Color::LightCyan, rit::Color::DarkGrey, sm_x, sm_y+sm_h-1);
		for (int c = 1; c < sm_w-1; c++)
			rit::System::draw_char('\xCD', rit::Color::LightCyan, rit::Color::DarkGrey, sm_x+c, sm_y+sm_h-1);
		rit::System::draw_char('\xBC', rit::Color::LightCyan, rit::Color::DarkGrey, sm_x+sm_w-1, sm_y+sm_h-1);

		// ── App items at y=14–20 (idx 0–6) ──
		const char menu_icon_chars[] = { '\xB0', '\xF1', '\xF0', '\x04', '\x09', '\x33', '\x0F' };
		const char* menu_labels[] = {
			" SysMonitor ", " Calculator ", " Text Editor",
			" File Explor", " Clock      ", " Calendar   ", " Settings   "
		};
		rit::Color icon_colors[] = {
			rit::Color::LightGreen,  rit::Color::LightBrown,
			rit::Color::White,       rit::Color::LightBrown,
			rit::Color::LightMagenta,rit::Color::LightRed,
			rit::Color::LightMagenta
		};

		for (int i = 0; i < 7; i++) {
			int iy = sm_y + 1 + i;   // y = 14..20
			bool hov = (m_mouse_x >= sm_x+1 && m_mouse_x < sm_x+sm_w-1 && m_mouse_y == iy);
			rit::Color ibg = hov ? rit::Color::Cyan    : rit::Color::DarkGrey;
			rit::Color ifg = hov ? rit::Color::Black   : rit::Color::White;

			rit::System::draw_char(menu_icon_chars[i], icon_colors[i], ibg, sm_x+1, iy);
			int ci = 0;
			while (menu_labels[i][ci]) {
				rit::System::draw_char(menu_labels[i][ci], ifg, ibg, sm_x+2+ci, iy);
				ci++;
			}
			for (int x2 = ci; x2 < sm_w-3; x2++)
				rit::System::draw_char(' ', ifg, ibg, sm_x+2+x2, iy);
		}

		// ── Divider at y=21 (idx=7) ──
		rit::System::draw_char('\xC7', rit::Color::LightCyan, rit::Color::DarkGrey, sm_x, sm_y+8);
		for (int c = 1; c < sm_w-1; c++)
			rit::System::draw_char('\xC4', rit::Color::DarkGrey, rit::Color::DarkGrey, sm_x+c, sm_y+8);
		rit::System::draw_char('\xB6', rit::Color::LightCyan, rit::Color::DarkGrey, sm_x+sm_w-1, sm_y+8);

		// ── Shutdown at y=22 (idx=8) ──
		{
			int iy = sm_y + 9;   // y = 22
			bool hov = (m_mouse_x >= sm_x+1 && m_mouse_x < sm_x+sm_w-1 && m_mouse_y == iy);
			rit::Color ibg = hov ? rit::Color::Red    : rit::Color::DarkGrey;
			rit::Color ifg = hov ? rit::Color::White  : rit::Color::LightRed;
			rit::System::draw_char('\x1E', ifg, ibg, sm_x+1, iy);
			draw_str(" Shut Down      ", ifg, ibg, sm_x+2, iy);
		}
	}
}

void Desktop::cycle_focus() {
	m_focus_index++;
	if (m_focus_index > 2) m_focus_index = 1;
	draw(); draw_cursor();
}

void Desktop::trigger_focused_action() {
	if      (m_focus_index == 1) rit::System::shutdown();
	else if (m_focus_index == 2) rit::System::reboot();
}

Window* Desktop::get_active_window() {
	for (int i = m_window_count-1; i >= 0; i--) {
		if (m_windows[i] && m_windows[i]->is_visible() &&
		    !m_windows[i]->is_minimized() && m_windows[i]->is_active())
			return m_windows[i];
	}
	return nullptr;
}

} // namespace ritos

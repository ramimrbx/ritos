#include "../include/ritos/apps.hpp"
#include "../include/ritos/desktop.hpp"
#include "../../framework/include/rit/vfs.hpp"

namespace ritos {

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


static void str_copy(char* dst, const char* src, int max_len) {
	int i = 0;
	while (src[i] != '\0' && i < max_len - 1) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

// ==========================================
// 1. SystemMonitorWindow Implementation
// ==========================================
SystemMonitorWindow::SystemMonitorWindow(int x, int y, int w, int h)
	: Window("System Monitor", x, y, w, h) {
	m_body_color = rit::Color::Black;
}

void SystemMonitorWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	int base_x = m_x + 2;
	int base_y = m_y + 1;

	int h = 0, m = 0, s = 0;
	rit::System::get_time(h, m, s);

	char val_buf[32];
	size_t heap_usage = rit::System::get_heap_usage();
	int_to_str(heap_usage, val_buf);

	int line = 0;
	const char* titles[] = {
		"OS Name:   RitOS v0.3.0",
		"CPU Arch:  x86 (32-bit)",
		"Compiler:  gcc-elf (C++)",
		"Filesystem:RAM Virtual FS",
		"Allocated: ",
		"SYS Time:  "
	};

	for (int i = 0; i < 6; i++) {
		int col = 0;
		while (titles[i][col] != '\0') {
			rit::System::draw_char(titles[i][col], rit::Color::LightCyan, m_body_color, base_x + col, base_y + line);
			col++;
		}
		
		if (i == 4) { // Heap usage
			int vcol = 0;
			while (val_buf[vcol] != '\0') {
				rit::System::draw_char(val_buf[vcol], rit::Color::White, m_body_color, base_x + col + vcol, base_y + line);
				vcol++;
			}
			const char* bytes_suffix = " bytes";
			int scol = 0;
			while (bytes_suffix[scol] != '\0') {
				rit::System::draw_char(bytes_suffix[scol], rit::Color::LightGrey, m_body_color, base_x + col + vcol + scol, base_y + line);
				scol++;
			}
		} else if (i == 5) { // Time
			char time_buf[16];
			time_buf[0] = (h / 10) + '0';
			time_buf[1] = (h % 10) + '0';
			time_buf[2] = ':';
			time_buf[3] = (m / 10) + '0';
			time_buf[4] = (m % 10) + '0';
			time_buf[5] = ':';
			time_buf[6] = (s / 10) + '0';
			time_buf[7] = (s % 10) + '0';
			time_buf[8] = '\0';
			int tcol = 0;
			while (time_buf[tcol] != '\0') {
				rit::System::draw_char(time_buf[tcol], rit::Color::LightGreen, m_body_color, base_x + col + tcol, base_y + line);
				tcol++;
			}
		}
		line += 1;
		if (line >= m_height - 3) break;
	}
}

// ==========================================
// 2. CalculatorWindow Implementation
// ==========================================
CalculatorWindow::CalculatorWindow(int x, int y)
	: Window("Calculator", x, y, 22, 11), m_accumulator(0), m_current_val(0), m_op('\0'), m_clear_on_next(false) {
	m_body_color = rit::Color::Black;
	m_display[0] = '0';
	m_display[1] = '\0';
}

void CalculatorWindow::update_display() {
	int_to_str(m_current_val, m_display);
}

void CalculatorWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	int disp_x = m_x + 2;
	int disp_y = m_y + 1;
	for (int col = 0; col < 18; col++) {
		rit::System::draw_char(' ', rit::Color::Black, rit::Color::DarkGrey, disp_x + col, disp_y);
	}
	int len = 0;
	while (m_display[len] != '\0') len++;
	int start_c = 17 - len;
	if (start_c < 0) start_c = 0;
	for (int i = 0; i < len; i++) {
		rit::System::draw_char(m_display[i], rit::Color::White, rit::Color::DarkGrey, disp_x + start_c + i, disp_y);
	}

	const char* buttons[4] = {
		" 7   8   9   / ",
		" 4   5   6   * ",
		" 1   2   3   - ",
		" C   0   =   + "
	};

	for (int r = 0; r < 4; r++) {
		int row_y = m_y + 3 + r * 2;
		for (int c = 0; c < 4; c++) {
			int btn_x = m_x + 2 + c * 5;
			rit::System::draw_char('[', rit::Color::DarkGrey, m_body_color, btn_x, row_y);
			rit::System::draw_char(buttons[r][c * 4 + 1], rit::Color::White, m_body_color, btn_x + 1, row_y);
			rit::System::draw_char(']', rit::Color::DarkGrey, m_body_color, btn_x + 2, row_y);
		}
	}
}

void CalculatorWindow::handle_click(int mx, int my) {
	int rx = mx - m_x;
	int ry = my - m_y;

	if (ry >= 3 && ry <= 9 && (ry - 3) % 2 == 0) {
		int row = (ry - 3) / 2;
		int col = (rx - 2) / 5;
		if (col >= 0 && col < 4 && (rx - 2) % 5 <= 2) {
			const char buttons_map[4][4] = {
				{'7', '8', '9', '/'},
				{'4', '5', '6', '*'},
				{'1', '2', '3', '-'},
				{'C', '0', '=', '+'}
			};
			char clicked_btn = buttons_map[row][col];

			if (clicked_btn >= '0' && clicked_btn <= '9') {
				if (m_clear_on_next) {
					m_current_val = 0;
					m_clear_on_next = false;
				}
				if (m_current_val < 9999999) {
					m_current_val = m_current_val * 10 + (clicked_btn - '0');
				}
				update_display();
			} else if (clicked_btn == 'C') {
				m_accumulator = 0;
				m_current_val = 0;
				m_op = '\0';
				m_clear_on_next = false;
				update_display();
			} else if (clicked_btn == '=') {
				if (m_op != '\0') {
					if (m_op == '+') m_current_val = m_accumulator + m_current_val;
					else if (m_op == '-') m_current_val = m_accumulator - m_current_val;
					else if (m_op == '*') m_current_val = m_accumulator * m_current_val;
					else if (m_op == '/') {
						if (m_current_val != 0) m_current_val = m_accumulator / m_current_val;
						else m_current_val = 99999999;
					}
					m_op = '\0';
					m_clear_on_next = true;
					update_display();
				}
			} else {
				m_accumulator = m_current_val;
				m_op = clicked_btn;
				m_clear_on_next = true;
			}
		}
	}
}

// ==========================================
// 3. TextEditorWindow Implementation
// ==========================================
TextEditorWindow::TextEditorWindow(int x, int y, int w, int h)
	: Window("Text Editor", x, y, w, h), m_buf_len(0) {
	m_body_color = rit::Color::Black;
	m_buffer[0] = '\0';
	m_filename[0] = '\0';
}

void TextEditorWindow::open_file(const char* filename) {
	int i = 0;
	while (filename[i] != '\0' && i < 31) {
		m_filename[i] = filename[i];
		i++;
	}
	m_filename[i] = '\0';

	const char* content = rit::VFS::read_file(m_filename);
	m_buf_len = 0;
	if (content != nullptr) {
		while (content[m_buf_len] != '\0' && m_buf_len < 511) {
			m_buffer[m_buf_len] = content[m_buf_len];
			m_buf_len++;
		}
	}
	m_buffer[m_buf_len] = '\0';
}

void TextEditorWindow::save_file() {
	if (m_filename[0] == '\0') {
		// Default name if blank
		str_copy(m_filename, "/notes/untitled.txt", 32);
		rit::VFS::create_file(m_filename, m_buffer);
	} else {
		if (!rit::VFS::exists(m_filename)) {
			rit::VFS::create_file(m_filename, m_buffer);
		} else {
			rit::VFS::write_file(m_filename, m_buffer);
		}
	}
}

void TextEditorWindow::draw() {
	if (!m_visible || m_minimized) return;
	
	// Draw Window Base but override dynamic title bar!
	Window::draw();

	// Draw custom file title bar (row m_y)
	rit::Color header_bg = m_active ? rit::Color::Blue : rit::Color::DarkGrey;
	// Draw custom title centered
	char title_buf[64];
	int t_len = 0;
	const char* base_title = "Text Editor - ";
	while (base_title[t_len] != '\0') {
		title_buf[t_len] = base_title[t_len];
		t_len++;
	}
	int fn_len = 0;
	const char* fn_src = m_filename[0] != '\0' ? m_filename : "[Untitled]";
	while (fn_src[fn_len] != '\0') {
		title_buf[t_len + fn_len] = fn_src[fn_len];
		fn_len++;
	}
	title_buf[t_len + fn_len] = '\0';
	t_len += fn_len;

	int title_x = m_x + (m_width - t_len) / 2;
	if (title_x > m_x && title_x < m_x + m_width - 1) {
		for (int i = 0; i < t_len; i++) {
			rit::System::draw_char(title_buf[i], m_title_color, header_bg, title_x + i, m_y);
		}
	}

	int base_x = m_x + 2;
	int base_y = m_y + 1;
	int max_w = m_width - 4;
	int max_h = m_height - 3;

	int cur_col = 0;
	int cur_row = 0;

	for (int i = 0; i < m_buf_len; i++) {
		char c = m_buffer[i];
		if (c == '\n') {
			cur_col = 0;
			cur_row++;
		} else {
			if (cur_col >= max_w) {
				cur_col = 0;
				cur_row++;
			}
			if (cur_row < max_h) {
				rit::System::draw_char(c, rit::Color::White, m_body_color, base_x + cur_col, base_y + cur_row);
			}
			cur_col++;
		}
		if (cur_row >= max_h) break;
	}

	if (m_active && cur_row < max_h) {
		rit::System::draw_char('_', rit::Color::LightCyan, m_body_color, base_x + cur_col, base_y + cur_row);
	}

	// Draw Action Buttons at the bottom line (inside body)
	int btn_y = m_y + m_height - 2;
	// Draw Save button [Save]
	const char* s_btn = "[ Save ]";
	int i = 0;
	while (s_btn[i] != '\0') {
		rit::System::draw_char(s_btn[i], rit::Color::White, rit::Color::Blue, m_x + 2 + i, btn_y);
		i++;
	}
}

void TextEditorWindow::handle_key(char key) {
	if (key == '\b') {
		if (m_buf_len > 0) {
			m_buf_len--;
			m_buffer[m_buf_len] = '\0';
		}
	} else if (key == '\t') {
		if (m_buf_len < 509) {
			m_buffer[m_buf_len++] = ' ';
			m_buffer[m_buf_len++] = ' ';
			m_buffer[m_buf_len] = '\0';
		}
	} else if (key == '\n') {
		if (m_buf_len < 510) {
			m_buffer[m_buf_len++] = '\n';
			m_buffer[m_buf_len] = '\0';
		}
	} else {
		if (m_buf_len < 510 && (uint8_t)key >= 32 && (uint8_t)key < 127) {
			m_buffer[m_buf_len++] = key;
			m_buffer[m_buf_len] = '\0';
		}
	}
}

void TextEditorWindow::handle_click(int mx, int my) {
	int rx = mx - m_x;
	int ry = my - m_y;

	// Check Save button click (y = height - 2, x relative 2 to 9)
	if (ry == m_height - 2) {
		if (rx >= 2 && rx <= 9) {
			save_file();
		}
	}
}

// ==========================================
// 4. ClockWindow Implementation
// ==========================================
ClockWindow::ClockWindow(int x, int y)
	: Window("Clock", x, y, 26, 8) {
	m_body_color = rit::Color::Black;
}

void ClockWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	int base_x = m_x + 2;
	int base_y = m_y + 1;

	int hr = 0, mn = 0, sc = 0;
	rit::System::get_time(hr, mn, sc);

	int yr = 0, mo = 0, dy = 0;
	rit::System::get_date(yr, mo, dy);

	// Render Time Centered
	const char* time_lbl = "TIME: ";
	int col = 0;
	while (time_lbl[col] != '\0') {
		rit::System::draw_char(time_lbl[col], rit::Color::LightCyan, m_body_color, base_x + col, base_y + 1);
		col++;
	}

	char time_str[10];
	time_str[0] = (hr / 10) + '0';
	time_str[1] = (hr % 10) + '0';
	time_str[2] = ':';
	time_str[3] = (mn / 10) + '0';
	time_str[4] = (mn % 10) + '0';
	time_str[5] = ':';
	time_str[6] = (sc / 10) + '0';
	time_str[7] = (sc % 10) + '0';
	time_str[8] = '\0';

	int tcol = 0;
	while (time_str[tcol] != '\0') {
		rit::System::draw_char(time_str[tcol], rit::Color::LightGreen, m_body_color, base_x + col + tcol, base_y + 1);
		tcol++;
	}

	// Render Date Centered
	const char* date_lbl = "DATE: ";
	col = 0;
	while (date_lbl[col] != '\0') {
		rit::System::draw_char(date_lbl[col], rit::Color::LightCyan, m_body_color, base_x + col, base_y + 3);
		col++;
	}

	char date_str[12];
	// Add 2000 to RTC year
	int full_year = 2000 + yr;
	date_str[0] = (full_year / 1000) + '0';
	date_str[1] = ((full_year / 100) % 10) + '0';
	date_str[2] = ((full_year / 10) % 10) + '0';
	date_str[3] = (full_year % 10) + '0';
	date_str[4] = '-';
	date_str[5] = (mo / 10) + '0';
	date_str[6] = (mo % 10) + '0';
	date_str[7] = '-';
	date_str[8] = (dy / 10) + '0';
	date_str[9] = (dy % 10) + '0';
	date_str[10] = '\0';

	int dcol = 0;
	while (date_str[dcol] != '\0') {
		rit::System::draw_char(date_str[dcol], rit::Color::White, m_body_color, base_x + col + dcol, base_y + 3);
		dcol++;
	}
}

// ==========================================
// 5. CalendarWindow Implementation
// ==========================================
CalendarWindow::CalendarWindow(int x, int y)
	: Window("Calendar", x, y, 26, 11) {
	m_body_color = rit::Color::Black;
}

// Sakamoto's algorithm for day of the week
static int day_of_week(int y, int m, int d) {
	static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	if (m < 3) y -= 1;
	return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

static int get_days_in_month(int year, int month) {
	if (month == 2) {
		if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) return 29;
		return 28;
	}
	if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
	return 31;
}

void CalendarWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	int base_x = m_x + 2;
	int base_y = m_y + 1;

	int yr = 0, mo = 0, dy = 0;
	rit::System::get_date(yr, mo, dy);
	int full_year = 2000 + yr;

	// Month Header
	const char* month_names[] = {
		"Invalid", "January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	};
	
	// Print Month and Year: e.g. "June 2026"
	char head_buf[32];
	int h_len = 0;
	const char* m_name = (mo >= 1 && mo <= 12) ? month_names[mo] : "Month";
	while (m_name[h_len] != '\0') {
		head_buf[h_len] = m_name[h_len];
		h_len++;
	}
	head_buf[h_len++] = ' ';
	char yr_buf[8];
	int_to_str(full_year, yr_buf);
	int y_idx = 0;
	while (yr_buf[y_idx] != '\0') {
		head_buf[h_len++] = yr_buf[y_idx++];
	}
	head_buf[h_len] = '\0';

	// Center Month Header
	int start_h = (22 - h_len) / 2;
	for (int i = 0; i < h_len; i++) {
		rit::System::draw_char(head_buf[i], rit::Color::LightCyan, m_body_color, base_x + start_h + i, base_y);
	}

	// Weekdays
	const char* days_hdr = "Su Mo Tu We Th Fr Sa";
	for (int i = 0; days_hdr[i] != '\0'; i++) {
		rit::System::draw_char(days_hdr[i], rit::Color::LightGrey, m_body_color, base_x + 1 + i, base_y + 1);
	}

	// Calculate calendar grid
	int start_day = day_of_week(full_year, mo, 1);
	int days_in_month = get_days_in_month(full_year, mo);

	int grid_row = 0;
	int grid_col = start_day;

	for (int day = 1; day <= days_in_month; day++) {
		int cell_x = base_x + 1 + grid_col * 3;
		int cell_y = base_y + 2 + grid_row;

		char day_buf[4];
		int_to_str(day, day_buf);
		
		rit::Color fg = (day == dy) ? rit::Color::Black : rit::Color::White;
		rit::Color bg = (day == dy) ? rit::Color::LightGreen : m_body_color;

		// Print day number (right-aligned in 2 char space)
		if (day < 10) {
			rit::System::draw_char(' ', fg, bg, cell_x, cell_y);
			rit::System::draw_char(day_buf[0], fg, bg, cell_x + 1, cell_y);
		} else {
			rit::System::draw_char(day_buf[0], fg, bg, cell_x, cell_y);
			rit::System::draw_char(day_buf[1], fg, bg, cell_x + 1, cell_y);
		}

		grid_col++;
		if (grid_col >= 7) {
			grid_col = 0;
			grid_row++;
		}
	}
}

// ==========================================
// 6. FileExplorerWindow Implementation
// ==========================================
FileExplorerWindow::FileExplorerWindow(int x, int y, TextEditorWindow* editor)
	: Window("File Explorer", x, y, 38, 16), m_selected_idx(0), m_file_count(0), m_editor(editor),
	  m_rename_mode(false), m_rename_len(0), m_active_pane(1), m_left_selected_idx(0) {
	m_body_color = rit::Color::Black;
	m_rename_buf[0] = '\0';
	refresh_files();
}

void FileExplorerWindow::refresh_files() {
	const char* temp_list[16];
	int total = rit::VFS::get_file_list(temp_list, 16);
	m_file_count = 0;
	for (int i = 0; i < total; i++) {
		const char* filename = temp_list[i];
		bool is_notes = (filename[0] == '/' && filename[1] == 'n' && filename[2] == 'o' && filename[3] == 't' && filename[4] == 'e' && filename[5] == 's');
		bool is_sys = (filename[0] == '/' && filename[1] == 's' && filename[2] == 'y' && filename[3] == 's');
		if (m_left_selected_idx == 0) { // / -> Show all
			m_file_list[m_file_count++] = filename;
		} else if (m_left_selected_idx == 1) { // sys -> Show under /sys/
			if (is_sys) {
				m_file_list[m_file_count++] = filename;
			}
		} else if (m_left_selected_idx == 2) { // notes -> Show under /notes/
			if (is_notes) {
				m_file_list[m_file_count++] = filename;
			}
		}
	}
}

void FileExplorerWindow::draw() {
	if (!m_visible || m_minimized) return;
	
	// Refresh file list before draw to keep it in sync with VFS
	refresh_files();
	Window::draw();

	int base_x = m_x + 2;
	int base_y = m_y + 1;

	// 1. Draw Menu Bar (ry = 1)
	const char* menu_bar = " File  Edit  View  Help";
	int col = 0;
	while (menu_bar[col] != '\0') {
		rit::System::draw_char(menu_bar[col], rit::Color::Black, rit::Color::LightGrey, base_x + col, base_y);
		col++;
	}
	// Fill rest of the menu bar line
	for (int x = col; x < m_width - 4; x++) {
		rit::System::draw_char(' ', rit::Color::Black, rit::Color::LightGrey, base_x + x, base_y);
	}

	// 2. Draw Address Bar (ry = 2, 3, 4)
	int ab_y = base_y + 1;
	// Top border of Address box
	rit::System::draw_char('\xDA', rit::Color::DarkGrey, m_body_color, base_x, ab_y);
	for (int i = 1; i < m_width - 5; i++) {
		rit::System::draw_char('\xC4', rit::Color::DarkGrey, m_body_color, base_x + i, ab_y);
	}
	rit::System::draw_char('\xBF', rit::Color::DarkGrey, m_body_color, base_x + m_width - 5, ab_y);

	// Address text line (ry = 3)
	rit::System::draw_char('\xB3', rit::Color::DarkGrey, m_body_color, base_x, ab_y + 1);
	const char* addr_lbl = " Address: /";
	if (m_left_selected_idx == 1) addr_lbl = " Address: /sys";
	else if (m_left_selected_idx == 2) addr_lbl = " Address: /notes";
	col = 0;
	while (addr_lbl[col] != '\0') {
		rit::System::draw_char(addr_lbl[col], rit::Color::LightGrey, m_body_color, base_x + 1 + col, ab_y + 1);
		col++;
	}
	for (int x = col; x < m_width - 6; x++) {
		rit::System::draw_char(' ', rit::Color::LightGrey, m_body_color, base_x + 1 + x, ab_y + 1);
	}
	rit::System::draw_char('\xB3', rit::Color::DarkGrey, m_body_color, base_x + m_width - 5, ab_y + 1);

	// Bottom border of Address box
	rit::System::draw_char('\xC0', rit::Color::DarkGrey, m_body_color, base_x, ab_y + 2);
	for (int i = 1; i < m_width - 5; i++) {
		rit::System::draw_char('\xC4', rit::Color::DarkGrey, m_body_color, base_x + i, ab_y + 2);
	}
	rit::System::draw_char('\xD9', rit::Color::DarkGrey, m_body_color, base_x + m_width - 5, ab_y + 2);

	// 3. Draw Columns Header (ry = 5)
	int hdr_y = base_y + 4;
	const char* col_hdr = " Name           Size    Type";
	col = 0;
	while (col_hdr[col] != '\0') {
		rit::System::draw_char(col_hdr[col], rit::Color::LightCyan, m_body_color, base_x + col, hdr_y);
		col++;
	}
	for (int x = col; x < m_width - 4; x++) {
		rit::System::draw_char(' ', rit::Color::LightCyan, m_body_color, base_x + x, hdr_y);
	}

	// 4. Draw Header Separator (ry = 6)
	int sep_y = base_y + 5;
	for (int i = 0; i < m_width - 4; i++) {
		rit::System::draw_char('\xC4', rit::Color::DarkGrey, m_body_color, base_x + i, sep_y);
	}

	// 5. Draw Left Pane (Tree) (ry = 7 to 13)
	int tree_start_y = base_y + 6;
	for (int row = 0; row < 7; row++) {
		int line_y = tree_start_y + row;
		if (row == 0) {
			const char* mc_lbl = " File System";
			int x = 0;
			while (mc_lbl[x] != '\0') {
				rit::System::draw_char(mc_lbl[x], rit::Color::LightGrey, m_body_color, base_x + x, line_y);
				x++;
			}
			for (; x < 11; x++) rit::System::draw_char(' ', rit::Color::LightGrey, m_body_color, base_x + x, line_y);
		} else if (row == 1) {
			bool is_sel = (m_active_pane == 0 && m_left_selected_idx == 0);
			rit::Color bg = is_sel ? rit::Color::Blue : m_body_color;
			rit::Color fg = is_sel ? rit::Color::White : rit::Color::LightGrey;
			rit::System::draw_char(' ', fg, bg, base_x, line_y);
			rit::System::draw_char('\xC3', fg, bg, base_x + 1, line_y);
			rit::System::draw_char('\xC4', fg, bg, base_x + 2, line_y);
			rit::System::draw_char('/', fg, bg, base_x + 3, line_y);
			for (int x = 4; x < 11; x++) rit::System::draw_char(' ', fg, bg, base_x + x, line_y);
		} else if (row == 2) {
			bool is_sel = (m_active_pane == 0 && m_left_selected_idx == 1);
			rit::Color bg = is_sel ? rit::Color::Blue : m_body_color;
			rit::Color fg = is_sel ? rit::Color::White : rit::Color::LightGrey;
			rit::System::draw_char(' ', fg, bg, base_x, line_y);
			rit::System::draw_char('\xB3', fg, bg, base_x + 1, line_y);
			rit::System::draw_char(' ', fg, bg, base_x + 2, line_y);
			rit::System::draw_char('\xC3', fg, bg, base_x + 3, line_y);
			rit::System::draw_char('\xC4', fg, bg, base_x + 4, line_y);
			rit::System::draw_char('\x0E', is_sel ? rit::Color::LightGreen : rit::Color::LightCyan, bg, base_x + 5, line_y);
			const char* sys_lbl = "sys";
			int x = 0;
			while (sys_lbl[x] != '\0' && x < 3) {
				rit::System::draw_char(sys_lbl[x], fg, bg, base_x + 6 + x, line_y);
				x++;
			}
			for (int col_idx = 6 + x; col_idx < 11; col_idx++) rit::System::draw_char(' ', fg, bg, base_x + col_idx, line_y);
		} else if (row == 3) {
			bool is_sel = (m_active_pane == 0 && m_left_selected_idx == 2);
			rit::Color bg = is_sel ? rit::Color::Blue : m_body_color;
			rit::Color fg = is_sel ? rit::Color::White : rit::Color::LightGrey;
			rit::System::draw_char(' ', fg, bg, base_x, line_y);
			rit::System::draw_char('\xB3', fg, bg, base_x + 1, line_y);
			rit::System::draw_char(' ', fg, bg, base_x + 2, line_y);
			rit::System::draw_char('\xC0', fg, bg, base_x + 3, line_y);
			rit::System::draw_char('\xC4', fg, bg, base_x + 4, line_y);
			rit::System::draw_char('\x0E', is_sel ? rit::Color::LightGreen : rit::Color::LightCyan, bg, base_x + 5, line_y);
			const char* n_lbl = "notes";
			int x = 0;
			while (n_lbl[x] != '\0' && x < 5) {
				rit::System::draw_char(n_lbl[x], fg, bg, base_x + 6 + x, line_y);
				x++;
			}
			for (int col_idx = 6 + x; col_idx < 11; col_idx++) rit::System::draw_char(' ', fg, bg, base_x + col_idx, line_y);
		} else {
			for (int x = 0; x < 11; x++) {
				rit::System::draw_char(' ', m_body_color, m_body_color, base_x + x, line_y);
			}
		}
	}

	// Draw Divider (ry = 7 to 13)
	for (int row = 0; row < 7; row++) {
		rit::System::draw_char('\xB3', rit::Color::DarkGrey, m_body_color, base_x + 11, tree_start_y + row);
	}

	// Draw Right Pane (Files Listing) (ry = 7 to 13)
	int right_base_x = base_x + 12;
	int print_h = 7;
	for (int i = 0; i < print_h; i++) {
		int line_y = tree_start_y + i;
		if (i < m_file_count) {
			bool is_selected = (m_active_pane == 1 && i == m_selected_idx);
			rit::Color bg = is_selected ? rit::Color::Blue : m_body_color;
			rit::Color fg = is_selected ? rit::Color::White : rit::Color::LightGrey;

			rit::System::draw_char('\x0E', is_selected ? rit::Color::LightGreen : rit::Color::LightCyan, bg, right_base_x + 1, line_y);
			rit::System::draw_char(' ', fg, bg, right_base_x + 2, line_y);

			int name_col = 0;
			// Strip directory prefix in listing to look like a subfolder content!
			const char* display_name = m_file_list[i];
			if (m_left_selected_idx == 1 && display_name[0] == '/' && display_name[1] == 's' && display_name[2] == 'y' && display_name[3] == 's' && display_name[4] == '/') {
				display_name += 5; // Skip "/sys/"
			} else if (m_left_selected_idx == 2 && display_name[0] == '/' && display_name[1] == 'n' && display_name[2] == 'o' && display_name[3] == 't' && display_name[4] == 'e' && display_name[5] == 's' && display_name[6] == '/') {
				display_name += 7; // Skip "/notes/"
			}

			while (display_name[name_col] != '\0' && name_col < 11) {
				rit::System::draw_char(display_name[name_col], fg, bg, right_base_x + 3 + name_col, line_y);
				name_col++;
			}
			for (int x = name_col; x < 11; x++) {
				rit::System::draw_char(' ', fg, bg, right_base_x + 3 + x, line_y);
			}
			rit::System::draw_char(' ', fg, bg, right_base_x + 14, line_y);

			const char* content = rit::VFS::read_file(m_file_list[i]);
			int f_size = 0;
			if (content != nullptr) {
				while (content[f_size] != '\0') f_size++;
			}
			char size_buf[8];
			int_to_str(f_size, size_buf);

			int sz_len = 0;
			while (size_buf[sz_len] != '\0') sz_len++;
			int sz_pad = 5 - sz_len;
			if (sz_pad < 0) sz_pad = 0;
			for (int x = 0; x < sz_pad; x++) {
				rit::System::draw_char(' ', fg, bg, right_base_x + 15 + x, line_y);
			}
			for (int x = 0; x < sz_len; x++) {
				rit::System::draw_char(size_buf[x], fg, bg, right_base_x + 15 + sz_pad + x, line_y);
			}
			rit::System::draw_char('B', fg, bg, right_base_x + 20, line_y);
			rit::System::draw_char(' ', fg, bg, right_base_x + 21, line_y);

			const char* type_str = "TXT";
			int type_col = 0;
			while (type_str[type_col] != '\0' && type_col < 2) {
				rit::System::draw_char(type_str[type_col], fg, bg, right_base_x + 22 + type_col, line_y);
				type_col++;
			}
			rit::System::draw_char(' ', fg, bg, right_base_x + 23, line_y);
		} else {
			for (int x = 0; x < 24; x++) {
				rit::System::draw_char(' ', m_body_color, m_body_color, right_base_x + x, line_y);
			}
		}
	}

	// 6. Draw Status Bar (ry = 13)
	int stat_y = base_y + 13;
	char stat_buf[48];
	int s_len = 0;
	char f_count_buf[8];
	int_to_str(m_file_count, f_count_buf);
	int fc_len = 0;
	while (f_count_buf[fc_len] != '\0') fc_len++;

	stat_buf[s_len++] = ' ';
	for (int x = 0; x < fc_len; x++) stat_buf[s_len++] = f_count_buf[x];
	const char* obj_lbl = " object(s)     Disk: ";
	int o_idx = 0;
	while (obj_lbl[o_idx] != '\0') stat_buf[s_len++] = obj_lbl[o_idx++];
	for (int x = 0; x < fc_len; x++) stat_buf[s_len++] = f_count_buf[x];
	const char* max_lbl = "/16 files";
	int m_idx = 0;
	while (max_lbl[m_idx] != '\0') stat_buf[s_len++] = max_lbl[m_idx++];
	stat_buf[s_len] = '\0';

	col = 0;
	while (stat_buf[col] != '\0') {
		rit::System::draw_char(stat_buf[col], rit::Color::LightGrey, rit::Color::DarkGrey, base_x + col, stat_y);
		col++;
	}
	for (int x = col; x < m_width - 4; x++) {
		rit::System::draw_char(' ', rit::Color::LightGrey, rit::Color::DarkGrey, base_x + x, stat_y);
	}

	// 7. Action buttons (ry = 14)
	int btn_y = m_y + m_height - 2;
	const char* btn_open = "[Open]";
	int btn_i = 0;
	while (btn_open[btn_i] != '\0') {
		rit::System::draw_char(btn_open[btn_i], rit::Color::White, rit::Color::Blue, m_x + 2 + btn_i, btn_y);
		btn_i++;
	}

	const char* btn_new = "[New]";
	btn_i = 0;
	while (btn_new[btn_i] != '\0') {
		rit::System::draw_char(btn_new[btn_i], rit::Color::White, rit::Color::Green, m_x + 10 + btn_i, btn_y);
		btn_i++;
	}

	const char* btn_rename = "[Rename]";
	btn_i = 0;
	while (btn_rename[btn_i] != '\0') {
		rit::System::draw_char(btn_rename[btn_i], rit::Color::White, rit::Color::DarkGrey, m_x + 17 + btn_i, btn_y);
		btn_i++;
	}

	const char* btn_delete = "[Delete]";
	btn_i = 0;
	while (btn_delete[btn_i] != '\0') {
		rit::System::draw_char(btn_delete[btn_i], rit::Color::White, rit::Color::Red, m_x + 27 + btn_i, btn_y);
		btn_i++;
	}

	// 8. Draw Rename Modal Dialog Box on top of everything
	if (m_rename_mode) {
		int dlg_x = m_x + 4;
		int dlg_y = m_y + 4;
		int dlg_w = 30;
		int dlg_h = 8;

		rit::System::draw_char('\xDA', rit::Color::White, rit::Color::DarkGrey, dlg_x, dlg_y);
		for (int c = 1; c < dlg_w - 1; c++) {
			rit::System::draw_char('\xC4', rit::Color::White, rit::Color::DarkGrey, dlg_x + c, dlg_y);
		}
		rit::System::draw_char('\xBF', rit::Color::White, rit::Color::DarkGrey, dlg_x + dlg_w - 1, dlg_y);

		for (int r = 1; r < dlg_h - 1; r++) {
			rit::System::draw_char('\xB3', rit::Color::White, rit::Color::DarkGrey, dlg_x, dlg_y + r);
			for (int c = 1; c < dlg_w - 1; c++) {
				rit::System::draw_char(' ', rit::Color::White, rit::Color::DarkGrey, dlg_x + c, dlg_y + r);
			}
			rit::System::draw_char('\xB3', rit::Color::White, rit::Color::DarkGrey, dlg_x + dlg_w - 1, dlg_y + r);
		}

		rit::System::draw_char('\xC0', rit::Color::White, rit::Color::DarkGrey, dlg_x, dlg_y + dlg_h - 1);
		for (int c = 1; c < dlg_w - 1; c++) {
			rit::System::draw_char('\xC4', rit::Color::White, rit::Color::DarkGrey, dlg_x + c, dlg_y + dlg_h - 1);
		}
		rit::System::draw_char('\xD9', rit::Color::White, rit::Color::DarkGrey, dlg_x + dlg_w - 1, dlg_y + dlg_h - 1);

		const char* dlg_title = " Rename File ";
		int t_col = 0;
		while (dlg_title[t_col] != '\0') {
			rit::System::draw_char(dlg_title[t_col], rit::Color::White, rit::Color::Blue, dlg_x + 2 + t_col, dlg_y + 1);
			t_col++;
		}

		const char* prompt = "Enter new filename:";
		int p_col = 0;
		while (prompt[p_col] != '\0') {
			rit::System::draw_char(prompt[p_col], rit::Color::White, rit::Color::DarkGrey, dlg_x + 2 + p_col, dlg_y + 3);
			p_col++;
		}

		rit::System::draw_char('[', rit::Color::White, rit::Color::Black, dlg_x + 2, dlg_y + 4);
		for (int c = 0; c < 24; c++) {
			char ch = ' ';
			if (c < m_rename_len) {
				ch = m_rename_buf[c];
			} else if (c == m_rename_len) {
				ch = '_';
			}
			rit::System::draw_char(ch, rit::Color::White, rit::Color::Black, dlg_x + 3 + c, dlg_y + 4);
		}
		rit::System::draw_char(']', rit::Color::White, rit::Color::Black, dlg_x + 27, dlg_y + 4);

		const char* btn_ok = "[  OK  ]";
		int ok_col = 0;
		while (btn_ok[ok_col] != '\0') {
			rit::System::draw_char(btn_ok[ok_col], rit::Color::White, rit::Color::Blue, dlg_x + 4 + ok_col, dlg_y + 6);
			ok_col++;
		}

		const char* btn_cancel = "[Cancel]";
		int cc_col = 0;
		while (btn_cancel[cc_col] != '\0') {
			rit::System::draw_char(btn_cancel[cc_col], rit::Color::White, rit::Color::Blue, dlg_x + 16 + cc_col, dlg_y + 6);
			cc_col++;
		}
	}
}

void FileExplorerWindow::handle_click(int mx, int my) {
	int rx = mx - m_x;
	int ry = my - m_y;

	if (m_rename_mode) {
		if (ry == 10) {
			// [ OK ] click (rx: 8 to 15)
			if (rx >= 8 && rx <= 15) {
				if (m_rename_len > 0 && m_selected_idx >= 0 && m_selected_idx < m_file_count) {
					m_rename_buf[m_rename_len] = '\0';
					rit::VFS::rename_file(m_file_list[m_selected_idx], m_rename_buf);
					refresh_files();
					m_rename_mode = false;
				}
			}
			// [Cancel] click (rx: 20 to 27)
			else if (rx >= 20 && rx <= 27) {
				m_rename_mode = false;
			}
		}
		return;
	}

	// Click inside panes (ry = 7 to 13)
	if (ry >= 7 && ry <= 13) {
		if (rx >= 2 && rx <= 12) {
			// Left Pane (Tree) click
			int clicked_folder = ry - 8;
			if (clicked_folder >= 0 && clicked_folder <= 2) {
				m_left_selected_idx = clicked_folder;
				m_active_pane = 0;
				m_selected_idx = 0;
				refresh_files();
			}
		} else if (rx >= 13 && rx <= 35) {
			// Right Pane (Files list) click
			int clicked_idx = ry - 7;
			if (clicked_idx < m_file_count) {
				m_selected_idx = clicked_idx;
				m_active_pane = 1;
			}
		}
	}
	// Action buttons clicks (ry = height - 2, which is 14)
	else if (ry == m_height - 2) {
		// [Open] click (rx: 2 to 7)
		if (rx >= 2 && rx <= 7) {
			if (m_selected_idx >= 0 && m_selected_idx < m_file_count) {
				m_editor->open_file(m_file_list[m_selected_idx]);
				m_editor->set_visible(true);
				m_editor->set_minimized(false);
				m_editor->set_active(true);
			}
		}
		// [New] click (rx: 10 to 14)
		else if (rx >= 10 && rx <= 14) {
			char new_name[32];
			const char* prefix = "/notes/notes";
			int num = 1;
			while (num < 100) {
				int len = 0;
				while (prefix[len] != '\0') {
					new_name[len] = prefix[len];
					len++;
				}
				char num_buf[8];
				int_to_str(num, num_buf);
				int n_idx = 0;
				while (num_buf[n_idx] != '\0') {
					new_name[len++] = num_buf[n_idx++];
				}
				const char* suffix = ".txt";
				int s_idx = 0;
				while (suffix[s_idx] != '\0') {
					new_name[len++] = suffix[s_idx++];
				}
				new_name[len] = '\0';

				if (!rit::VFS::exists(new_name)) {
					break;
				}
				num++;
			}
			rit::VFS::create_file(new_name, "Write notes here...");
			m_left_selected_idx = 2; // Automatically switch to "/notes" folder
			m_active_pane = 1;
			refresh_files();
			m_selected_idx = m_file_count - 1;
			m_editor->open_file(new_name);
			m_editor->set_visible(true);
			m_editor->set_minimized(false);
			m_editor->set_active(true);
		}
		// [Rename] click (rx: 17 to 24)
		else if (rx >= 17 && rx <= 24) {
			if (m_selected_idx >= 0 && m_selected_idx < m_file_count) {
				m_rename_mode = true;
				str_copy(m_rename_buf, m_file_list[m_selected_idx], 32);
				m_rename_len = 0;
				while (m_rename_buf[m_rename_len] != '\0') m_rename_len++;
			}
		}
		// [Delete] click (rx: 27 to 34)
		else if (rx >= 27 && rx <= 34) {
			if (m_selected_idx >= 0 && m_selected_idx < m_file_count) {
				rit::VFS::delete_file(m_file_list[m_selected_idx]);
				refresh_files();
				m_selected_idx = 0;
			}
		}
	}
}

void FileExplorerWindow::handle_key(char key) {
	if (m_rename_mode) {
		if (key == '\b') {
			if (m_rename_len > 0) {
				m_rename_len--;
				m_rename_buf[m_rename_len] = '\0';
			}
		} else if (key == '\n') {
			if (m_rename_len > 0 && m_selected_idx >= 0 && m_selected_idx < m_file_count) {
				m_rename_buf[m_rename_len] = '\0';
				rit::VFS::rename_file(m_file_list[m_selected_idx], m_rename_buf);
				refresh_files();
				m_rename_mode = false;
			}
		} else if (key == 27) { // Esc
			m_rename_mode = false;
		} else if (m_rename_len < 24 && (uint8_t)key >= 32 && (uint8_t)key < 127) {
			m_rename_buf[m_rename_len++] = key;
			m_rename_buf[m_rename_len] = '\0';
		}
		return;
	}

	if (m_active_pane == 0) {
		if (key == 'w' || key == 0x1E) { // Up
			if (m_left_selected_idx > 0) {
				m_left_selected_idx--;
				m_selected_idx = 0;
				refresh_files();
			}
		} else if (key == 's' || key == 0x1F) { // Down
			if (m_left_selected_idx < 2) {
				m_left_selected_idx++;
				m_selected_idx = 0;
				refresh_files();
			}
		} else if (key == 'd' || key == 0x1D) { // Right
			if (m_file_count > 0) {
				m_active_pane = 1;
				m_selected_idx = 0;
			}
		}
	} else {
		if (key == 'w' || key == 0x1E) { // Up
			if (m_selected_idx > 0) {
				m_selected_idx--;
			}
		} else if (key == 's' || key == 0x1F) { // Down
			if (m_selected_idx < m_file_count - 1) {
				m_selected_idx++;
			}
		} else if (key == 'a' || key == 0x1C) { // Left
			m_active_pane = 0;
		} else if (key == '\n') { // Enter
			if (m_selected_idx >= 0 && m_selected_idx < m_file_count) {
				m_editor->open_file(m_file_list[m_selected_idx]);
				m_editor->set_visible(true);
				m_editor->set_minimized(false);
				m_editor->set_active(true);
			}
		}
	}
}

// ==========================================
// 7. SettingsWindow Implementation
// ==========================================
SettingsWindow::SettingsWindow(int x, int y, Desktop* desktop)
	: Window("Settings", x, y, 24, 11), m_desktop(desktop) {
	m_body_color = rit::Color::Black;
}

void SettingsWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	const char* t_lbl = "Wallpapers:";
	int col = 0;
	while (t_lbl[col] != '\0') {
		rit::System::draw_char(t_lbl[col], rit::Color::LightCyan, m_body_color, m_x + 2 + col, m_y + 1);
		col++;
	}

	const char* pat_btns[] = {
		"[Grid]", "[Solid]", "[Stars]"
	};
	int px = m_x + 2;
	for (int i = 0; i < 3; i++) {
		int col_btn = 0;
		while (pat_btns[i][col_btn] != '\0') {
			rit::System::draw_char(pat_btns[i][col_btn], rit::Color::White, rit::Color::DarkGrey, px + col_btn, m_y + 3);
			col_btn++;
		}
		px += col_btn + 1;
	}

	const char* th_lbl = "Themes:";
	col = 0;
	while (th_lbl[col] != '\0') {
		rit::System::draw_char(th_lbl[col], rit::Color::LightCyan, m_body_color, m_x + 2 + col, m_y + 5);
		col++;
	}

	const char* theme_btns[] = {
		"[Blue]", "[Dark]", "[Green]", "[Red]"
	};
	int tx = m_x + 2;
	for (int i = 0; i < 4; i++) {
		int col_btn = 0;
		while (theme_btns[i][col_btn] != '\0') {
			rit::System::draw_char(theme_btns[i][col_btn], rit::Color::White, rit::Color::DarkGrey, tx + col_btn, m_y + 7);
			col_btn++;
		}
		tx += col_btn + 1;
	}
}

void SettingsWindow::handle_click(int mx, int my) {
	int rx = mx - m_x;
	int ry = my - m_y;

	if (ry == 3) {
		if (rx >= 2 && rx <= 7) {
			m_desktop->set_bg_pattern('G');
		}
		else if (rx >= 9 && rx <= 15) {
			m_desktop->set_bg_pattern('S');
		}
		else if (rx >= 17 && rx <= 23) {
			m_desktop->set_bg_pattern('*');
		}
	}
	else if (ry == 7) {
		if (rx >= 2 && rx <= 7) {
			m_desktop->set_bg_color(rit::Color::Blue);
		}
		else if (rx >= 9 && rx <= 14) {
			m_desktop->set_bg_color(rit::Color::DarkGrey);
		}
		else if (rx >= 16 && rx <= 22) {
			m_desktop->set_bg_color(rit::Color::Green);
		}
		else if (rx >= 24 && rx <= 28) {
			m_desktop->set_bg_color(rit::Color::Red);
		}
	}
}

} // namespace ritos

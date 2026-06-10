#include "../include/ritos/apps.hpp"
#include "../include/ritos/desktop.hpp"
#include "../../framework/include/rit/vfs.hpp"

namespace ritos {

// ── Shared helpers ──────────────────────────────────────────────────────────

static void int_to_str(int n, char* buf) {
	int i = 0;
	bool neg = n < 0;
	if (neg) n = -n;
	if (n == 0) { buf[i++] = '0'; }
	else { while (n > 0) { buf[i++] = (n%10)+'0'; n /= 10; } }
	if (neg) buf[i++] = '-';
	buf[i] = '\0';
	for (int j = 0; j < i/2; j++) { char t=buf[j]; buf[j]=buf[i-j-1]; buf[i-j-1]=t; }
}

static void str_copy(char* dst, const char* src, int max_len) {
	int i = 0;
	while (src[i] && i < max_len-1) { dst[i]=src[i]; i++; }
	dst[i] = '\0';
}

static void draw_str(const char* s, rit::Color fg, rit::Color bg, int x, int y) {
	for (int i = 0; s[i]; i++) rit::System::draw_char(s[i], fg, bg, x+i, y);
}

// Draw a horizontal separator line inside a window body
static void draw_hline(rit::Color border_c, rit::Color body_c, int wx, int wy, int w, int row) {
	rit::System::draw_char('\xC3', border_c, body_c, wx, wy+row);
	for (int i = 1; i < w-1; i++)
		rit::System::draw_char('\xC4', rit::Color::DarkGrey, body_c, wx+i, wy+row);
	rit::System::draw_char('\xB4', border_c, body_c, wx+w-1, wy+row);
}

// ── 1. System Monitor ────────────────────────────────────────────────────────

SystemMonitorWindow::SystemMonitorWindow(int x, int y, int w, int h)
	: Window("System Monitor", x, y, w, h) {
	m_body_color = rit::Color::Black;
}

void SystemMonitorWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	rit::Color bc = m_active ? rit::Color::LightCyan : rit::Color::DarkGrey;
	int bx = m_x+2, by = m_y+1;

	// Section header
	draw_str("\x10 System", rit::Color::LightCyan, m_body_color, bx, by);

	draw_hline(bc, m_body_color, m_x, m_y, m_width, 2);

	int h=0, mn=0, s=0; rit::System::get_time(h, mn, s);
	size_t heap = rit::System::get_heap_usage();
	char hbuf[16]; int_to_str((int)heap, hbuf);

	const char* labels[] = {
		"OS Name : ", "CPU Arch : ", "Compiler : ",
		"Filesys  : ", "Heap Used: ", "Sys Time : "
	};
	const char* vals[] = {
		"RitOS v0.3.0", "x86 (32-bit)", "gcc-elf (C++)",
		"RAM VirtFS", nullptr, nullptr
	};
	rit::Color val_colors[] = {
		rit::Color::LightGreen, rit::Color::LightGreen,
		rit::Color::LightGreen, rit::Color::LightGreen,
		rit::Color::White, rit::Color::LightCyan
	};

	for (int i = 0; i < 6; i++) {
		int row = by + 2 + i;
		if (row >= m_y + m_height - 1) break;
		draw_str(labels[i], rit::Color::LightCyan, m_body_color, bx, row);
		int lx = bx + 10;
		if (i == 4) {
			draw_str(hbuf, rit::Color::White, m_body_color, lx, row);
			draw_str(" bytes", rit::Color::LightGrey, m_body_color, lx+8, row);
		} else if (i == 5) {
			char tb[9];
			tb[0]=(h/10)+'0'; tb[1]=(h%10)+'0'; tb[2]=':';
			tb[3]=(mn/10)+'0'; tb[4]=(mn%10)+'0'; tb[5]=':';
			tb[6]=(s/10)+'0'; tb[7]=(s%10)+'0'; tb[8]='\0';
			draw_str(tb, rit::Color::LightCyan, m_body_color, lx, row);
		} else {
			draw_str(vals[i], val_colors[i], m_body_color, lx, row);
		}
	}
}

// ── 2. Calculator ────────────────────────────────────────────────────────────

CalculatorWindow::CalculatorWindow(int x, int y)
	: Window("Calculator", x, y, 22, 11),
	  m_accumulator(0), m_current_val(0), m_op('\0'), m_clear_on_next(false) {
	m_body_color = rit::Color::Black;
	m_display[0] = '0'; m_display[1] = '\0';
}

void CalculatorWindow::update_display() { int_to_str(m_current_val, m_display); }

void CalculatorWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	int bx = m_x+2, by = m_y+1;

	// Display (dark grey box, right-aligned white number)
	for (int c = 0; c < 18; c++)
		rit::System::draw_char(' ', rit::Color::DarkGrey, rit::Color::DarkGrey, bx+c, by);
	int len = 0; while (m_display[len]) len++;
	int sc = 17 - len; if (sc < 0) sc = 0;
	for (int i = 0; i < len; i++)
		rit::System::draw_char(m_display[i], rit::Color::LightGreen, rit::Color::DarkGrey, bx+sc+i, by);

	// Current op indicator
	if (m_op) {
		rit::System::draw_char(m_op, rit::Color::LightCyan, rit::Color::DarkGrey, bx, by);
	}

	// Button grid  [row][col]
	// Digit buttons: DarkGrey bg, White fg
	// Operator buttons: Blue bg, LightCyan fg
	// Clear: Red-ish (LightRed fg), Equals: Green fg
	const char btns[4][4] = {
		{'7','8','9','/'},
		{'4','5','6','*'},
		{'1','2','3','-'},
		{'C','0','=','+'}
	};
	// is_operator: 0=digit, 1=op, 2=clear, 3=equals
	const int btn_type[4][4] = {
		{0,0,0,1},
		{0,0,0,1},
		{0,0,0,1},
		{2,0,3,1}
	};

	for (int r = 0; r < 4; r++) {
		int ry = m_y + 3 + r*2;
		for (int c = 0; c < 4; c++) {
			int bx2 = m_x + 2 + c*5;
			rit::Color fg, bracket_fg;
			switch (btn_type[r][c]) {
				case 1:  fg = rit::Color::LightCyan;  bracket_fg = rit::Color::Blue;      break;
				case 2:  fg = rit::Color::LightRed;   bracket_fg = rit::Color::DarkGrey;  break;
				case 3:  fg = rit::Color::LightGreen; bracket_fg = rit::Color::DarkGrey;  break;
				default: fg = rit::Color::White;       bracket_fg = rit::Color::DarkGrey;  break;
			}
			rit::System::draw_char('[', bracket_fg, m_body_color, bx2,   ry);
			rit::System::draw_char(btns[r][c], fg, m_body_color, bx2+1, ry);
			rit::System::draw_char(']', bracket_fg, m_body_color, bx2+2, ry);
		}
	}
}

void CalculatorWindow::handle_click(int mx, int my) {
	int rx = mx - m_x, ry = my - m_y;
	if (ry >= 3 && ry <= 9 && (ry-3)%2 == 0) {
		int row = (ry-3)/2, col = (rx-2)/5;
		if (col >= 0 && col < 4 && (rx-2)%5 <= 2) {
			const char bmap[4][4] = {{'7','8','9','/'},{'4','5','6','*'},{'1','2','3','-'},{'C','0','=','+'}};
			char btn = bmap[row][col];
			if (btn >= '0' && btn <= '9') {
				if (m_clear_on_next) { m_current_val = 0; m_clear_on_next = false; }
				if (m_current_val < 9999999) m_current_val = m_current_val*10 + (btn-'0');
				update_display();
			} else if (btn == 'C') {
				m_accumulator = 0; m_current_val = 0; m_op = '\0'; m_clear_on_next = false;
				update_display();
			} else if (btn == '=') {
				if (m_op != '\0') {
					if      (m_op == '+') m_current_val = m_accumulator + m_current_val;
					else if (m_op == '-') m_current_val = m_accumulator - m_current_val;
					else if (m_op == '*') m_current_val = m_accumulator * m_current_val;
					else if (m_op == '/') m_current_val = m_current_val ? m_accumulator/m_current_val : 99999999;
					m_op = '\0'; m_clear_on_next = true; update_display();
				}
			} else {
				m_accumulator = m_current_val; m_op = btn; m_clear_on_next = true;
			}
		}
	}
}

// ── 3. Text Editor ───────────────────────────────────────────────────────────

TextEditorWindow::TextEditorWindow(int x, int y, int w, int h)
	: Window("Text Editor", x, y, w, h), m_buf_len(0) {
	m_body_color = rit::Color::Black;
	m_buffer[0] = '\0'; m_filename[0] = '\0';
}

void TextEditorWindow::open_file(const char* filename) {
	int i = 0; while (filename[i] && i < 31) { m_filename[i]=filename[i]; i++; } m_filename[i]='\0';
	const char* content = rit::VFS::read_file(m_filename);
	m_buf_len = 0;
	if (content) while (content[m_buf_len] && m_buf_len < 511) { m_buffer[m_buf_len]=content[m_buf_len]; m_buf_len++; }
	m_buffer[m_buf_len] = '\0';
}

void TextEditorWindow::save_file() {
	if (m_filename[0] == '\0') {
		str_copy(m_filename, "/notes/untitled.txt", 32);
		rit::VFS::create_file(m_filename, m_buffer);
	} else {
		if (!rit::VFS::exists(m_filename)) rit::VFS::create_file(m_filename, m_buffer);
		else rit::VFS::write_file(m_filename, m_buffer);
	}
}

void TextEditorWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	rit::Color hbg = m_active ? rit::Color::Blue : rit::Color::DarkGrey;

	// Dynamic title: "Editor – filename"
	char tbuf[64]; int tlen = 0;
	const char* pre = "Editor \xF7 ";
	while (pre[tlen]) { tbuf[tlen]=pre[tlen]; tlen++; }
	const char* fn = m_filename[0] ? m_filename : "[Untitled]";
	int fi = 0; while (fn[fi]) { tbuf[tlen++]=fn[fi++]; }
	tbuf[tlen] = '\0';
	int tx = m_x + (m_width - tlen) / 2;
	if (tx > m_x && tx < m_x+m_width-1) {
		for (int i = 0; i < tlen; i++)
			rit::System::draw_char(tbuf[i], rit::Color::White, hbg, tx+i, m_y);
	}

	int bx = m_x+2, by = m_y+1;
	int maxw = m_width-4, maxh = m_height-3;
	int cc = 0, cr = 0;

	for (int i = 0; i < m_buf_len; i++) {
		char c = m_buffer[i];
		if (c == '\n') { cc=0; cr++; }
		else {
			if (cc >= maxw) { cc=0; cr++; }
			if (cr < maxh) rit::System::draw_char(c, rit::Color::White, m_body_color, bx+cc, by+cr);
			cc++;
		}
		if (cr >= maxh) break;
	}
	// Blinking cursor
	if (m_active && cr < maxh)
		rit::System::draw_char('_', rit::Color::LightCyan, m_body_color, bx+cc, by+cr);

	// Status bar line (second-to-last row)
	int sbary = m_y + m_height - 2;
	for (int c = 1; c < m_width-1; c++)
		rit::System::draw_char(' ', rit::Color::LightGrey, rit::Color::DarkGrey, m_x+c, sbary);
	rit::System::draw_char('\xC3', rit::Color::DarkGrey, rit::Color::DarkGrey, m_x, sbary);
	rit::System::draw_char('\xB4', rit::Color::DarkGrey, rit::Color::DarkGrey, m_x+m_width-1, sbary);

	// Save button
	const char* sbtn = "[ Save ]";
	int si = 0; while (sbtn[si]) {
		rit::System::draw_char(sbtn[si], rit::Color::White, rit::Color::Blue, m_x+2+si, sbary);
		si++;
	}
	// Char count
	char cbuf[16]; int_to_str(m_buf_len, cbuf);
	draw_str(" chars:", rit::Color::LightGrey, rit::Color::DarkGrey, m_x+12, sbary);
	draw_str(cbuf, rit::Color::LightCyan, rit::Color::DarkGrey, m_x+19, sbary);
}

void TextEditorWindow::handle_key(char key) {
	if      (key == '\b') { if (m_buf_len > 0) { m_buf_len--; m_buffer[m_buf_len]='\0'; } }
	else if (key == '\t') { if (m_buf_len < 509) { m_buffer[m_buf_len++]=' '; m_buffer[m_buf_len++]=' '; m_buffer[m_buf_len]='\0'; } }
	else if (key == '\n') { if (m_buf_len < 510) { m_buffer[m_buf_len++]='\n'; m_buffer[m_buf_len]='\0'; } }
	else if (m_buf_len < 510 && (uint8_t)key >= 32 && (uint8_t)key < 127) { m_buffer[m_buf_len++]=key; m_buffer[m_buf_len]='\0'; }
}

void TextEditorWindow::handle_click(int mx, int my) {
	int rx = mx-m_x, ry = my-m_y;
	if (ry == m_height-2 && rx >= 2 && rx <= 9) save_file();
}

// ── 4. Clock ─────────────────────────────────────────────────────────────────

ClockWindow::ClockWindow(int x, int y)
	: Window("Clock", x, y, 26, 9) {
	m_body_color = rit::Color::Black;
}

void ClockWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	int bx = m_x+2, by = m_y+1;
	int hr=0, mn=0, sc=0; rit::System::get_time(hr, mn, sc);
	int yr=0, mo=0, dy=0; rit::System::get_date(yr, mo, dy);

	// Large time display with border
	rit::System::draw_char('\xDA', rit::Color::DarkGrey, m_body_color, bx,    by);
	for (int c=1; c<20; c++) rit::System::draw_char('\xC4', rit::Color::DarkGrey, m_body_color, bx+c, by);
	rit::System::draw_char('\xBF', rit::Color::DarkGrey, m_body_color, bx+20, by);

	rit::System::draw_char('\xB3', rit::Color::DarkGrey, m_body_color, bx, by+1);
	draw_str("  TIME  ", rit::Color::LightCyan, m_body_color, bx+1, by+1);

	char ts[9];
	ts[0]=(hr/10)+'0'; ts[1]=(hr%10)+'0'; ts[2]=':';
	ts[3]=(mn/10)+'0'; ts[4]=(mn%10)+'0'; ts[5]=':';
	ts[6]=(sc/10)+'0'; ts[7]=(sc%10)+'0'; ts[8]='\0';
	draw_str(ts, rit::Color::LightGreen, m_body_color, bx+9, by+1);
	rit::System::draw_char('\xB3', rit::Color::DarkGrey, m_body_color, bx+20, by+1);

	rit::System::draw_char('\xC3', rit::Color::DarkGrey, m_body_color, bx,    by+2);
	for (int c=1; c<20; c++) rit::System::draw_char('\xC4', rit::Color::DarkGrey, m_body_color, bx+c, by+2);
	rit::System::draw_char('\xB4', rit::Color::DarkGrey, m_body_color, bx+20, by+2);

	rit::System::draw_char('\xB3', rit::Color::DarkGrey, m_body_color, bx, by+3);
	draw_str("  DATE  ", rit::Color::LightCyan, m_body_color, bx+1, by+3);

	int fy = 2000 + yr;
	char ds[11];
	ds[0]=(fy/1000)+'0'; ds[1]=((fy/100)%10)+'0'; ds[2]=((fy/10)%10)+'0'; ds[3]=(fy%10)+'0';
	ds[4]='-'; ds[5]=(mo/10)+'0'; ds[6]=(mo%10)+'0';
	ds[7]='-'; ds[8]=(dy/10)+'0'; ds[9]=(dy%10)+'0'; ds[10]='\0';
	draw_str(ds, rit::Color::White, m_body_color, bx+9, by+3);
	rit::System::draw_char('\xB3', rit::Color::DarkGrey, m_body_color, bx+20, by+3);

	rit::System::draw_char('\xC0', rit::Color::DarkGrey, m_body_color, bx,    by+4);
	for (int c=1; c<20; c++) rit::System::draw_char('\xC4', rit::Color::DarkGrey, m_body_color, bx+c, by+4);
	rit::System::draw_char('\xD9', rit::Color::DarkGrey, m_body_color, bx+20, by+4);

	// Second progress bar
	int bar_len = (sc * 20) / 60;
	for (int c = 0; c < 20; c++) {
		char bc = (c < bar_len) ? '\xDB' : '\xB0';
		rit::Color fc = (c < bar_len) ? rit::Color::LightCyan : rit::Color::DarkGrey;
		rit::System::draw_char(bc, fc, m_body_color, bx+c, by+6);
	}
}

// ── 5. Calendar ───────────────────────────────────────────────────────────────

CalendarWindow::CalendarWindow(int x, int y)
	: Window("Calendar", x, y, 26, 12) {
	m_body_color = rit::Color::Black;
}

static int day_of_week(int y, int m, int d) {
	static int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
	if (m < 3) y -= 1;
	return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

static int get_days_in_month(int year, int month) {
	if (month == 2) return ((year%4==0 && year%100!=0) || year%400==0) ? 29 : 28;
	if (month==4||month==6||month==9||month==11) return 30;
	return 31;
}

void CalendarWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	int bx = m_x+2, by = m_y+1;
	int yr=0, mo=0, dy=0; rit::System::get_date(yr, mo, dy);
	int fy = 2000 + yr;

	const char* mnames[] = {
		"Invalid","January","February","March","April","May","June",
		"July","August","September","October","November","December"
	};
	char hd[32]; int hl = 0;
	const char* mn = (mo>=1&&mo<=12) ? mnames[mo] : "Month";
	while (mn[hl]) { hd[hl]=mn[hl]; hl++; }
	hd[hl++] = ' ';
	char ybuf[8]; int_to_str(fy, ybuf);
	int yi=0; while (ybuf[yi]) hd[hl++]=ybuf[yi++];
	hd[hl]='\0';

	// Month/Year centered in LightCyan
	int sh = (22-hl)/2;
	draw_str(hd, rit::Color::LightCyan, m_body_color, bx+sh, by);

	// Weekday header in LightGrey
	const char* whdr = "Su Mo Tu We Th Fr Sa";
	draw_str(whdr, rit::Color::LightGrey, m_body_color, bx+1, by+1);

	// Divider
	for (int c=0; c<22; c++)
		rit::System::draw_char('\xC4', rit::Color::DarkGrey, m_body_color, bx+c, by+2);

	int start_day = day_of_week(fy, mo, 1);
	int dim = get_days_in_month(fy, mo);
	int grid_row = 0, grid_col = start_day;

	for (int day = 1; day <= dim; day++) {
		int cx = bx+1+grid_col*3, cy = by+3+grid_row;
		char dbuf[4]; int_to_str(day, dbuf);
		rit::Color fg = (day==dy) ? rit::Color::Black   : rit::Color::White;
		rit::Color bg = (day==dy) ? rit::Color::LightGreen : m_body_color;
		// Weekend highlight
		if (grid_col==0||grid_col==6) {
			if (day!=dy) fg = rit::Color::LightRed;
		}
		if (day < 10) {
			rit::System::draw_char(' ', fg, bg, cx,   cy);
			rit::System::draw_char(dbuf[0], fg, bg, cx+1, cy);
		} else {
			rit::System::draw_char(dbuf[0], fg, bg, cx,   cy);
			rit::System::draw_char(dbuf[1], fg, bg, cx+1, cy);
		}
		grid_col++;
		if (grid_col >= 7) { grid_col=0; grid_row++; }
	}
}

// ── 6. File Explorer ─────────────────────────────────────────────────────────

FileExplorerWindow::FileExplorerWindow(int x, int y, TextEditorWindow* editor)
	: Window("File Explorer", x, y, 38, 16),
	  m_selected_idx(0), m_file_count(0), m_editor(editor),
	  m_rename_mode(false), m_rename_len(0), m_active_pane(1), m_left_selected_idx(0) {
	m_body_color = rit::Color::Black;
	m_rename_buf[0] = '\0';
	refresh_files();
}

void FileExplorerWindow::refresh_files() {
	const char* tmp[16]; int total = rit::VFS::get_file_list(tmp, 16);
	m_file_count = 0;
	for (int i = 0; i < total; i++) {
		const char* f = tmp[i];
		bool is_sys   = f[0]=='/'&&f[1]=='s'&&f[2]=='y'&&f[3]=='s';
		bool is_notes = f[0]=='/'&&f[1]=='n'&&f[2]=='o'&&f[3]=='t'&&f[4]=='e'&&f[5]=='s';
		if      (m_left_selected_idx == 0)                m_file_list[m_file_count++] = f;
		else if (m_left_selected_idx == 1 && is_sys)      m_file_list[m_file_count++] = f;
		else if (m_left_selected_idx == 2 && is_notes)    m_file_list[m_file_count++] = f;
	}
}

void FileExplorerWindow::draw() {
	if (!m_visible || m_minimized) return;
	refresh_files();
	Window::draw();

	rit::Color bc = m_active ? rit::Color::LightCyan : rit::Color::DarkGrey;
	int bx = m_x+2, by = m_y+1;

	// ── Menu bar ──
	for (int c=0; c<m_width-4; c++)
		rit::System::draw_char(' ', rit::Color::Black, rit::Color::LightGrey, bx+c, by);
	draw_str("\x10 File  \x10 Edit  \x10 View  \x10 Help", rit::Color::Black, rit::Color::LightGrey, bx, by);

	// ── Address bar ──
	int aby = by+1;
	rit::System::draw_char('\xDA', rit::Color::DarkGrey, m_body_color, bx, aby);
	for (int c=1; c<m_width-5; c++) rit::System::draw_char('\xC4', rit::Color::DarkGrey, m_body_color, bx+c, aby);
	rit::System::draw_char('\xBF', rit::Color::DarkGrey, m_body_color, bx+m_width-5, aby);

	rit::System::draw_char('\xB3', rit::Color::DarkGrey, m_body_color, bx, aby+1);
	const char* addr = (m_left_selected_idx==1) ? " Path: /sys"
	                 : (m_left_selected_idx==2) ? " Path: /notes"
	                                             : " Path: /";
	draw_str(addr, rit::Color::LightGrey, m_body_color, bx+1, aby+1);
	for (int c=(int)0; c < m_width-6-(int)0; c++) {} // filler handled by Window::draw
	rit::System::draw_char('\xB3', rit::Color::DarkGrey, m_body_color, bx+m_width-5, aby+1);

	rit::System::draw_char('\xC0', rit::Color::DarkGrey, m_body_color, bx, aby+2);
	for (int c=1; c<m_width-5; c++) rit::System::draw_char('\xC4', rit::Color::DarkGrey, m_body_color, bx+c, aby+2);
	rit::System::draw_char('\xD9', rit::Color::DarkGrey, m_body_color, bx+m_width-5, aby+2);

	// ── Column header ──
	int hy = by+4;
	draw_str(" Name           Size    Type", rit::Color::LightCyan, m_body_color, bx, hy);
	for (int c=28; c<m_width-4; c++) rit::System::draw_char(' ', rit::Color::LightCyan, m_body_color, bx+c, hy);

	draw_hline(bc, m_body_color, m_x, m_y, m_width, hy - m_y + 1 - m_y + m_y); // adjusted
	// Actually draw separator directly:
	for (int c=1; c<m_width-1; c++) rit::System::draw_char('\xC4', rit::Color::DarkGrey, m_body_color, m_x+c, hy+1);
	rit::System::draw_char('\xC3', bc, m_body_color, m_x, hy+1);
	rit::System::draw_char('\xB4', bc, m_body_color, m_x+m_width-1, hy+1);

	// ── Left pane (folder tree) ──
	int ty = by+6;
	const char* tree_hdr = " \x0E FileSystem";
	draw_str(tree_hdr, rit::Color::LightGrey, m_body_color, bx, ty);

	struct { const char* label; rit::Color icon_fg; } folders[3] = {
		{" /       ", rit::Color::LightCyan},
		{" \x0E sys   ", rit::Color::LightGreen},
		{" \x0E notes ", rit::Color::LightGreen}
	};
	for (int r = 0; r < 3; r++) {
		bool sel = (m_active_pane==0 && m_left_selected_idx==r);
		rit::Color pbg = sel ? rit::Color::Blue : m_body_color;
		rit::Color pfg = sel ? rit::Color::White : rit::Color::LightGrey;
		int ly = ty+1+r;
		for (int c=0; c<11; c++) rit::System::draw_char(' ', pfg, pbg, bx+c, ly);
		int ci=0; while (folders[r].label[ci]) {
			rit::Color fc = (ci==1) ? folders[r].icon_fg : pfg;
			rit::System::draw_char(folders[r].label[ci], fc, pbg, bx+ci, ly);
			ci++;
		}
	}
	// Empty remaining left pane rows
	for (int r=3; r<7; r++) {
		int ly = ty+1+r;
		for (int c=0; c<11; c++) rit::System::draw_char(' ', m_body_color, m_body_color, bx+c, ly);
	}

	// Vertical divider
	for (int r=0; r<7; r++) rit::System::draw_char('\xB3', rit::Color::DarkGrey, m_body_color, bx+11, ty+r);

	// ── Right pane (file list) ──
	int rx = bx+12;
	for (int i=0; i<7; i++) {
		int ly = ty+i;
		if (i < m_file_count) {
			bool sel = (m_active_pane==1 && i==m_selected_idx);
			rit::Color pbg = sel ? rit::Color::Blue : m_body_color;
			rit::Color pfg = sel ? rit::Color::White : rit::Color::LightGrey;
			rit::Color ic  = sel ? rit::Color::LightGreen : rit::Color::LightCyan;

			rit::System::draw_char('\x04', ic, pbg, rx, ly); // ♦ icon
			rit::System::draw_char(' ', pfg, pbg, rx+1, ly);

			const char* dn = m_file_list[i];
			if (m_left_selected_idx==1 && dn[0]=='/'&&dn[1]=='s'&&dn[2]=='y'&&dn[3]=='s'&&dn[4]=='/') dn+=5;
			else if (m_left_selected_idx==2 && dn[0]=='/'&&dn[1]=='n'&&dn[2]=='o'&&dn[3]=='t'&&dn[4]=='e'&&dn[5]=='s'&&dn[6]=='/') dn+=7;

			int nc=0; while (dn[nc]&&nc<11) { rit::System::draw_char(dn[nc], pfg, pbg, rx+2+nc, ly); nc++; }
			for (int c=nc; c<11; c++) rit::System::draw_char(' ', pfg, pbg, rx+2+c, ly);
			rit::System::draw_char(' ', pfg, pbg, rx+13, ly);

			const char* cont = rit::VFS::read_file(m_file_list[i]);
			int fsz=0; if (cont) while (cont[fsz]) fsz++;
			char szb[8]; int_to_str(fsz, szb);
			int sl=0; while (szb[sl]) sl++;
			int sp=5-sl; if(sp<0)sp=0;
			for (int c=0;c<sp;c++) rit::System::draw_char(' ',pfg,pbg,rx+14+c,ly);
			for (int c=0;c<sl;c++) rit::System::draw_char(szb[c],pfg,pbg,rx+14+sp+c,ly);
			rit::System::draw_char('B', pfg, pbg, rx+19, ly);
			rit::System::draw_char(' ', pfg, pbg, rx+20, ly);
			draw_str("TXT", pfg, pbg, rx+21, ly);
		} else {
			for (int c=0; c<24; c++) rit::System::draw_char(' ', m_body_color, m_body_color, rx+c, ly);
		}
	}

	// ── Status bar ──
	int sty = by+13;
	for (int c=1; c<m_width-1; c++) rit::System::draw_char(' ', rit::Color::LightGrey, rit::Color::DarkGrey, m_x+c, sty);
	rit::System::draw_char('\xC3', bc, rit::Color::DarkGrey, m_x, sty);
	rit::System::draw_char('\xB4', bc, rit::Color::DarkGrey, m_x+m_width-1, sty);

	char fcb[8]; int_to_str(m_file_count, fcb);
	draw_str(" \xFA ", rit::Color::LightGreen, rit::Color::DarkGrey, m_x+1, sty);
	draw_str(fcb, rit::Color::White, rit::Color::DarkGrey, m_x+4, sty);
	draw_str(" files  \xB3  /16 capacity", rit::Color::LightGrey, rit::Color::DarkGrey, m_x+5+3, sty);

	// ── Action buttons ──
	int btny = m_y+m_height-2;
	const char* btns[] = {"[Open]","[New ]","[Rename]","[Delete]"};
	rit::Color bgs[] = { rit::Color::Blue, rit::Color::Green, rit::Color::DarkGrey, rit::Color::Red };
	int bpos[] = { 2, 10, 17, 27 };
	for (int b=0; b<4; b++) {
		int ci=0; while (btns[b][ci]) {
			rit::System::draw_char(btns[b][ci], rit::Color::White, bgs[b], m_x+bpos[b]+ci, btny);
			ci++;
		}
	}

	// ── Rename modal ──
	if (m_rename_mode) {
		int dx=m_x+4, dy2=m_y+4, dw=30, dh=8;
		rit::System::draw_char('\xC9', rit::Color::LightCyan, rit::Color::DarkGrey, dx, dy2);
		for (int c=1;c<dw-1;c++) rit::System::draw_char('\xCD', rit::Color::LightCyan, rit::Color::DarkGrey, dx+c, dy2);
		rit::System::draw_char('\xBB', rit::Color::LightCyan, rit::Color::DarkGrey, dx+dw-1, dy2);
		for (int r=1;r<dh-1;r++) {
			rit::System::draw_char('\xBA', rit::Color::LightCyan, rit::Color::DarkGrey, dx, dy2+r);
			for (int c=1;c<dw-1;c++) rit::System::draw_char(' ', rit::Color::White, rit::Color::DarkGrey, dx+c, dy2+r);
			rit::System::draw_char('\xBA', rit::Color::LightCyan, rit::Color::DarkGrey, dx+dw-1, dy2+r);
		}
		rit::System::draw_char('\xC8', rit::Color::LightCyan, rit::Color::DarkGrey, dx, dy2+dh-1);
		for (int c=1;c<dw-1;c++) rit::System::draw_char('\xCD', rit::Color::LightCyan, rit::Color::DarkGrey, dx+c, dy2+dh-1);
		rit::System::draw_char('\xBC', rit::Color::LightCyan, rit::Color::DarkGrey, dx+dw-1, dy2+dh-1);

		draw_str("\x0F Rename File", rit::Color::LightCyan, rit::Color::Blue, dx+2, dy2+1);
		for (int c=2;c<dw-1;c++) rit::System::draw_char(' ', rit::Color::LightCyan, rit::Color::Blue, dx+c, dy2+1);
		draw_str(" Enter new filename:", rit::Color::White, rit::Color::DarkGrey, dx+1, dy2+3);
		rit::System::draw_char('[', rit::Color::LightCyan, rit::Color::Black, dx+2, dy2+4);
		for (int c=0;c<24;c++) {
			char ch = (c<m_rename_len)?m_rename_buf[c]:(c==m_rename_len?'_':' ');
			rit::System::draw_char(ch, rit::Color::White, rit::Color::Black, dx+3+c, dy2+4);
		}
		rit::System::draw_char(']', rit::Color::LightCyan, rit::Color::Black, dx+27, dy2+4);
		draw_str("[  OK  ]", rit::Color::White, rit::Color::Blue, dx+4, dy2+6);
		draw_str("[Cancel]", rit::Color::White, rit::Color::Blue, dx+16, dy2+6);
	}
}

void FileExplorerWindow::handle_click(int mx, int my) {
	int rx=mx-m_x, ry=my-m_y;
	if (m_rename_mode) {
		if (ry==10) {
			if (rx>=8&&rx<=15 && m_rename_len>0 && m_selected_idx<m_file_count) {
				m_rename_buf[m_rename_len]='\0';
				rit::VFS::rename_file(m_file_list[m_selected_idx], m_rename_buf);
				refresh_files(); m_rename_mode=false;
			} else if (rx>=20&&rx<=27) { m_rename_mode=false; }
		}
		return;
	}
	if (ry>=7&&ry<=13) {
		if (rx>=2&&rx<=12) {
			int cf=ry-8;
			if (cf>=0&&cf<=2) { m_left_selected_idx=cf; m_active_pane=0; m_selected_idx=0; refresh_files(); }
		} else if (rx>=13&&rx<=35) {
			int ci=ry-7;
			if (ci<m_file_count) { m_selected_idx=ci; m_active_pane=1; }
		}
	} else if (ry==m_height-2) {
		if (rx>=2&&rx<=7) {
			if (m_selected_idx>=0&&m_selected_idx<m_file_count) {
				if (m_editor) { m_editor->open_file(m_file_list[m_selected_idx]); m_editor->set_visible(true); m_editor->set_minimized(false); m_editor->set_active(true); }
				else { rit::VFS::create_file("/sys/open_file.txt",m_file_list[m_selected_idx]); rit::System::launch_app("Text Editor"); }
			}
		} else if (rx>=10&&rx<=14) {
			char nn[32]; const char* pfx="/notes/notes"; int num=1;
			while (num<100) {
				int l=0; while (pfx[l]) { nn[l]=pfx[l]; l++; }
				char nb[8]; int_to_str(num,nb); int ni=0; while (nb[ni]) nn[l++]=nb[ni++];
				const char* sx=".txt"; int si=0; while (sx[si]) nn[l++]=sx[si++];
				nn[l]='\0';
				if (!rit::VFS::exists(nn)) break;
				num++;
			}
			rit::VFS::create_file(nn,"Write notes here...");
			m_left_selected_idx=2; m_active_pane=1; refresh_files(); m_selected_idx=m_file_count-1;
			if (m_editor) { m_editor->open_file(nn); m_editor->set_visible(true); m_editor->set_minimized(false); m_editor->set_active(true); }
			else { rit::VFS::create_file("/sys/open_file.txt",nn); rit::System::launch_app("Text Editor"); }
		} else if (rx>=17&&rx<=24) {
			if (m_selected_idx>=0&&m_selected_idx<m_file_count) {
				m_rename_mode=true; str_copy(m_rename_buf,m_file_list[m_selected_idx],32);
				m_rename_len=0; while (m_rename_buf[m_rename_len]) m_rename_len++;
			}
		} else if (rx>=27&&rx<=34) {
			if (m_selected_idx>=0&&m_selected_idx<m_file_count) {
				rit::VFS::delete_file(m_file_list[m_selected_idx]); refresh_files(); m_selected_idx=0;
			}
		}
	}
}

void FileExplorerWindow::handle_key(char key) {
	if (m_rename_mode) {
		if (key=='\b') { if (m_rename_len>0) m_rename_buf[--m_rename_len]='\0'; }
		else if (key=='\n') {
			if (m_rename_len>0&&m_selected_idx<m_file_count) {
				m_rename_buf[m_rename_len]='\0'; rit::VFS::rename_file(m_file_list[m_selected_idx],m_rename_buf);
				refresh_files(); m_rename_mode=false;
			}
		} else if (key==27) { m_rename_mode=false; }
		else if (m_rename_len<24&&(uint8_t)key>=32&&(uint8_t)key<127) { m_rename_buf[m_rename_len++]=key; m_rename_buf[m_rename_len]='\0'; }
		return;
	}
	if (m_active_pane==0) {
		if      (key=='w'||key==0x1E) { if (m_left_selected_idx>0) { m_left_selected_idx--; m_selected_idx=0; refresh_files(); } }
		else if (key=='s'||key==0x1F) { if (m_left_selected_idx<2) { m_left_selected_idx++; m_selected_idx=0; refresh_files(); } }
		else if (key=='d'||key==0x1D) { if (m_file_count>0) { m_active_pane=1; m_selected_idx=0; } }
	} else {
		if      (key=='w'||key==0x1E) { if (m_selected_idx>0) m_selected_idx--; }
		else if (key=='s'||key==0x1F) { if (m_selected_idx<m_file_count-1) m_selected_idx++; }
		else if (key=='a'||key==0x1C) { m_active_pane=0; }
		else if (key=='\n') {
			if (m_selected_idx>=0&&m_selected_idx<m_file_count) {
				if (m_editor) { m_editor->open_file(m_file_list[m_selected_idx]); m_editor->set_visible(true); m_editor->set_minimized(false); m_editor->set_active(true); }
				else { rit::VFS::create_file("/sys/open_file.txt",m_file_list[m_selected_idx]); rit::System::launch_app("Text Editor"); }
			}
		}
	}
}

// ── 7. Settings ───────────────────────────────────────────────────────────────

SettingsWindow::SettingsWindow(int x, int y, Desktop* desktop)
	: Window("Settings", x, y, 28, 13), m_desktop(desktop) {
	m_body_color = rit::Color::Black;
}

void SettingsWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();

	int bx = m_x+2, by = m_y+1;

	// Wallpaper section
	draw_str("\x0F Wallpaper Pattern:", rit::Color::LightCyan, m_body_color, bx, by);
	draw_hline(m_active?rit::Color::LightCyan:rit::Color::DarkGrey, m_body_color, m_x, m_y, m_width, 2);

	const char* pp[] = {"[Pic]  ","[Grid] ","[Solid]","[Stars]"};
	rit::Color ppbg[] = {rit::Color::DarkGrey, rit::Color::DarkGrey, rit::Color::DarkGrey, rit::Color::DarkGrey};
	int ppx = bx;
	for (int i=0; i<4; i++) {
		int ci=0; while (pp[i][ci]) {
			rit::System::draw_char(pp[i][ci], rit::Color::White, ppbg[i], ppx+ci, by+3);
			ci++;
		}
		ppx += ci + 1;
	}

	// Theme section
	draw_str("\x0F Desktop Theme:", rit::Color::LightCyan, m_body_color, bx, by+5);
	draw_hline(m_active?rit::Color::LightCyan:rit::Color::DarkGrey, m_body_color, m_x, m_y, m_width, 6);

	const char* tp[] = {"[Dark] ","[Blue] ","[Green]","[Red]  "};
	rit::Color tpfg[] = {rit::Color::LightGrey,rit::Color::LightBlue,rit::Color::LightGreen,rit::Color::LightRed};
	int tpx = bx;
	for (int i=0; i<4; i++) {
		int ci=0; while (tp[i][ci]) {
			rit::System::draw_char(tp[i][ci], tpfg[i], rit::Color::DarkGrey, tpx+ci, by+8);
			ci++;
		}
		tpx += ci + 1;
	}

	// Power section
	draw_hline(m_active?rit::Color::LightCyan:rit::Color::DarkGrey, m_body_color, m_x, m_y, m_width, 10);
	draw_str("[Shutdown]  [Reboot]", rit::Color::White, rit::Color::DarkGrey, bx, by+10);
}

void SettingsWindow::handle_click(int mx, int my) {
	int rx=mx-m_x, ry=my-m_y;

	char pattern='G', color_char='D';
	if (rit::VFS::exists("/sys/settings.cfg")) {
		const char* old = rit::VFS::read_file("/sys/settings.cfg");
		if (old&&old[0]&&old[1]) { pattern=old[0]; color_char=old[1]; }
	}

	if (ry==4) {
		if      (rx>=2&&rx<=7)   pattern='P';
		else if (rx>=9&&rx<=15)  pattern='G';
		else if (rx>=17&&rx<=23) pattern='S';
		else if (rx>=25&&rx<=31) pattern='*';
	} else if (ry==9) {
		if      (rx>=2&&rx<=8)   color_char='D';
		else if (rx>=10&&rx<=16) color_char='B';
		else if (rx>=18&&rx<=24) color_char='G';
		else if (rx>=26&&rx<=30) color_char='R';
	} else if (ry==11) {
		if (rx>=2&&rx<=10)  rit::System::shutdown();
		if (rx>=13&&rx<=20) rit::System::reboot();
	}

	if (m_desktop) {
		if      (pattern=='P') m_desktop->set_bg_pattern('P');
		else if (pattern=='G') m_desktop->set_bg_pattern('G');
		else if (pattern=='S') m_desktop->set_bg_pattern('S');
		else if (pattern=='*') m_desktop->set_bg_pattern('*');

		if      (color_char=='D') m_desktop->set_bg_color(rit::Color::Black);
		else if (color_char=='B') m_desktop->set_bg_color(rit::Color::Blue);
		else if (color_char=='G') m_desktop->set_bg_color(rit::Color::Green);
		else if (color_char=='R') m_desktop->set_bg_color(rit::Color::Red);
	} else {
		char cfg[3]={pattern,color_char,'\0'};
		rit::VFS::write_file("/sys/settings.cfg", cfg);
	}
}

} // namespace ritos

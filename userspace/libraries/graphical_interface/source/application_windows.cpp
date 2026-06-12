#include <ritos/application_windows.hpp>
#include <ritos/fluent.hpp>
#include <rit/virtual_filesystem.hpp>

extern const RitOS_API* g_api;

/*
 * Windows-11-styled built-in app windows. Everything is drawn in pixels
 * with fluent.hpp tokens; handle_click receives exact pixel coordinates.
 */

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

static bool in_rect(int mx, int my, int x, int y, int w, int h) {
	return mx >= x && mx < x + w && my >= y && my < y + h;
}

/* A "card": white rounded surface with a subtle stroke, Win11 style */
static void card(int x, int y, int w, int h) {
	fluent::rrect_border(x, y, w, h, 7, fluent::STROKE, fluent::CARD);
}

// ── 1. System Monitor ────────────────────────────────────────────────────────

SystemMonitorWindow::SystemMonitorWindow(int x, int y, int w, int h)
	: Window("System Monitor", x, y, w, h) {
	m_body_color = rit::Color::Black;
}

void SystemMonitorWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();
	if (!rit::System::fb_is_avail()) return;

	int bx = px() + 20, by = content_y() + 16, bw = pw() - 40;

	fluent::text("System", fluent::TEXT, bx, by);

	/* Info card */
	int cy = by + 28;
	card(bx, cy, bw, 130);
	const char* labels[] = { "OS name", "CPU architecture", "Compiler", "Filesystem" };
	const char* vals[]   = { "RitOS v0.4.0", "x86 (32-bit)", "gcc-elf (C++)", "RAM VirtFS" };
	for (int i = 0; i < 4; i++) {
		fluent::text(labels[i], fluent::TEXT_SEC, bx + 16, cy + 14 + i * 28);
		fluent::text(vals[i],   fluent::TEXT,     bx + bw / 2, cy + 14 + i * 28);
		if (i < 3) rit::System::fb_draw_hline_px(bx + 12, cy + 36 + i * 28, bw - 24, fluent::STROKE);
	}

	/* Memory card with usage bar */
	int my2 = cy + 142;
	card(bx, my2, bw, 84);
	fluent::text("Memory", fluent::TEXT_SEC, bx + 16, my2 + 12);

	size_t heap = rit::System::get_heap_usage();
	char hbuf[16]; int_to_str((int)(heap / 1024), hbuf);
	int hl = 0; while (hbuf[hl]) hl++;
	hbuf[hl] = ' '; hbuf[hl+1] = 'K'; hbuf[hl+2] = 'B'; hbuf[hl+3] = '\0';
	fluent::text(hbuf, fluent::TEXT, bx + bw - 16 - fluent::text_w(hbuf), my2 + 12);

	/* bar: heap usage out of 64MB cap */
	int bar_w = bw - 32;
	int used  = (int)(heap / 1024);          /* KB    */
	int frac  = used * bar_w / (64 * 1024);  /* of 64MB, 32-bit safe */
	if (frac > bar_w) frac = bar_w;
	if (frac < 2) frac = 2;
	fluent::rrect(bx + 16, my2 + 44, bar_w, 10, 5, fluent::STROKE);
	fluent::rrect(bx + 16, my2 + 44, frac, 10, 5, fluent::ACCENT);

	/* Clock card */
	int ty = my2 + 96;
	if (ty + 64 < py() + ph() - 8) {
		card(bx, ty, bw, 56);
		fluent::text("System time", fluent::TEXT_SEC, bx + 16, ty + 20);
		int h=0, mn=0, s=0; rit::System::get_time(h, mn, s);
		char tb[9];
		tb[0]=(h/10)+'0'; tb[1]=(h%10)+'0'; tb[2]=':';
		tb[3]=(mn/10)+'0'; tb[4]=(mn%10)+'0'; tb[5]=':';
		tb[6]=(s/10)+'0'; tb[7]=(s%10)+'0'; tb[8]='\0';
		fluent::text(tb, fluent::TEXT, bx + bw - 16 - fluent::text_w(tb), ty + 20);
	}
}

// ── 2. Calculator ────────────────────────────────────────────────────────────

/* 4 columns x 5 rows, Win11 layout-ish */
static const char k_calc_btns[5][4] = {
	{'C','%','^','/'},   /* % and ^ are decorative no-ops kept for looks */
	{'7','8','9','*'},
	{'4','5','6','-'},
	{'1','2','3','+'},
	{'~','0','.','='},   /* ~ = negate, . is a no-op (integer calc) */
};

#define CALC_PAD   12
#define CALC_DISP  64
#define CALC_GAP   4

CalculatorWindow::CalculatorWindow(int x, int y)
	: Window("Calculator", x, y, 42, 30),
	  m_accumulator(0), m_current_val(0), m_op('\0'), m_clear_on_next(false) {
	m_body_color = rit::Color::Black;
	m_display[0] = '0'; m_display[1] = '\0';
}

void CalculatorWindow::update_display() { int_to_str(m_current_val, m_display); }

void CalculatorWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();
	if (!rit::System::fb_is_avail()) return;

	int bx = px() + CALC_PAD, by = content_y() + 8;
	int bw = pw() - 2 * CALC_PAD;

	/* Display: right-aligned heading number */
	fluent::text(m_display, fluent::TEXT,
	             bx + bw - 8 - fluent::text_w(m_display, fluent::FONT_H2),
	             by + CALC_DISP - 44, fluent::FONT_H2);
	if (m_op) {
		char ob[2] = { m_op, 0 };
		fluent::text(ob, fluent::TEXT_SEC, bx + 4, by + CALC_DISP - 38);
	}

	/* Button grid */
	int gy = by + CALC_DISP;
	int btn_w = (bw - 3 * CALC_GAP) / 4;
	int btn_h = (content_h() - CALC_DISP - 16 - 4 * CALC_GAP) / 5;

	for (int r = 0; r < 5; r++) {
		for (int c = 0; c < 4; c++) {
			int x = bx + c * (btn_w + CALC_GAP);
			int y = gy + r * (btn_h + CALC_GAP);
			char b = k_calc_btns[r][c];
			bool digit  = (b >= '0' && b <= '9') || b == '.';
			bool equals = (b == '=');

			uint32_t bg = equals ? fluent::ACCENT
			            : digit  ? fluent::CARD
			                     : fluent::CARD_ALT;
			fluent::rrect_border(x, y, btn_w, btn_h, 5,
			                     equals ? fluent::ACCENT : fluent::BORDER_DIM, bg);

			char lb[4] = { b, 0, 0, 0 };
			if (b == '~') { lb[0]='+'; lb[1]='/'; lb[2]='-'; }
			uint32_t fg = equals ? fluent::TEXT_WHITE
			            : (b=='C') ? fluent::DANGER : fluent::TEXT;
			fluent::text(lb, fg, x + (btn_w - fluent::text_w(lb)) / 2,
			             y + (btn_h - 16) / 2);
		}
	}
}

void CalculatorWindow::press(char btn) {
	if (btn >= '0' && btn <= '9') {
		if (m_clear_on_next) { m_current_val = 0; m_clear_on_next = false; }
		if (m_current_val < 99999999 && m_current_val > -99999999)
			m_current_val = m_current_val*10 + (btn-'0');
		update_display();
	} else if (btn == 'C') {
		m_accumulator = 0; m_current_val = 0; m_op = '\0'; m_clear_on_next = false;
		update_display();
	} else if (btn == '~') {
		m_current_val = -m_current_val; update_display();
	} else if (btn == '=') {
		if (m_op != '\0') {
			if      (m_op == '+') m_current_val = m_accumulator + m_current_val;
			else if (m_op == '-') m_current_val = m_accumulator - m_current_val;
			else if (m_op == '*') m_current_val = m_accumulator * m_current_val;
			else if (m_op == '/') m_current_val = m_current_val ? m_accumulator/m_current_val : 99999999;
			m_op = '\0'; m_clear_on_next = true; update_display();
		}
	} else if (btn=='+'||btn=='-'||btn=='*'||btn=='/') {
		m_accumulator = m_current_val; m_op = btn; m_clear_on_next = true;
	}
}

void CalculatorWindow::handle_click(int mx, int my) {
	int bx = px() + CALC_PAD, by = content_y() + 8;
	int bw = pw() - 2 * CALC_PAD;
	int gy = by + CALC_DISP;
	int btn_w = (bw - 3 * CALC_GAP) / 4;
	int btn_h = (content_h() - CALC_DISP - 16 - 4 * CALC_GAP) / 5;

	for (int r = 0; r < 5; r++)
		for (int c = 0; c < 4; c++) {
			int x = bx + c * (btn_w + CALC_GAP);
			int y = gy + r * (btn_h + CALC_GAP);
			if (in_rect(mx, my, x, y, btn_w, btn_h)) { press(k_calc_btns[r][c]); return; }
		}
}

void CalculatorWindow::handle_key(char key) {
	if ((key >= '0' && key <= '9') || key=='+'||key=='-'||key=='*'||key=='/'||key=='=')
		press(key);
	else if (key == '\n') press('=');
	else if (key == 'c')  press('C');
}

// ── 3. Text Editor ───────────────────────────────────────────────────────────

#define TE_TOOLBAR 40
#define TE_STATUS  24

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
		str_copy(m_filename, "/users/ramim/documents/untitled.txt", 32);
		rit::VFS::create_file(m_filename, m_buffer);
	} else {
		if (!rit::VFS::exists(m_filename)) rit::VFS::create_file(m_filename, m_buffer);
		else rit::VFS::write_file(m_filename, m_buffer);
	}
	/* Files under /volumes/disk/ mirror the physical disk's FAT root */
	const char* pre = "/volumes/disk/";
	int i = 0; while (pre[i] && m_filename[i] == pre[i]) i++;
	if (!pre[i] && g_api && g_api->storage_export)
		g_api->storage_export(m_filename);
}

void TextEditorWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();
	if (!rit::System::fb_is_avail()) return;

	int wx = px(), wy = content_y(), ww = pw(), wh = content_h();

	/* Toolbar: mica strip with Save button and filename */
	fluent::rect(wx + 1, wy, ww - 2, TE_TOOLBAR, fluent::MICA);
	rit::System::fb_draw_hline_px(wx + 1, wy + TE_TOOLBAR - 1, ww - 2, fluent::STROKE);
	fluent::button(wx + 12, wy + 6, 72, 28, "Save", false, true);
	const char* fn = m_filename[0] ? m_filename : "Untitled";
	fluent::text(fn, fluent::TEXT_SEC, wx + 100, wy + 12);

	/* Text area */
	int tx = wx + 16, ty = wy + TE_TOOLBAR + 10;
	int maxw = (ww - 32) / 8;
	int maxh = (wh - TE_TOOLBAR - TE_STATUS - 16) / 18;
	int cc = 0, cr = 0;
	for (int i = 0; i < m_buf_len; i++) {
		char c = m_buffer[i];
		if (c == '\n') { cc=0; cr++; }
		else {
			if (cc >= maxw) { cc=0; cr++; }
			if (cr < maxh) {
				/* monospace grid: editor content stays on the 8x16 face */
				char tb[2] = { c, 0 };
				rit::System::fb_draw_string_px(tb, fluent::TEXT, 0,
				                               tx + cc*8, ty + cr*18, 1);
			}
			cc++;
		}
		if (cr >= maxh) break;
	}
	/* Caret */
	if (m_active && cr < maxh)
		fluent::rect(tx + cc*8, ty + cr*18, 1, 16, fluent::ACCENT);

	/* Status bar */
	int sy = wy + wh - TE_STATUS;
	fluent::rect(wx + 1, sy, ww - 2, TE_STATUS - 1, fluent::CARD_ALT);
	rit::System::fb_draw_hline_px(wx + 1, sy, ww - 2, fluent::STROKE);
	char cbuf[24]; int_to_str(m_buf_len, cbuf);
	int cl = 0; while (cbuf[cl]) cl++;
	const char* suffix = " characters";
	int si = 0; while (suffix[si]) { cbuf[cl+si] = suffix[si]; si++; }
	cbuf[cl+si] = '\0';
	fluent::text(cbuf, fluent::TEXT_SEC, wx + 14, sy + 4);
}

void TextEditorWindow::handle_key(char key) {
	if      (key == '\b') { if (m_buf_len > 0) { m_buf_len--; m_buffer[m_buf_len]='\0'; } }
	else if (key == '\t') { if (m_buf_len < 509) { m_buffer[m_buf_len++]=' '; m_buffer[m_buf_len++]=' '; m_buffer[m_buf_len]='\0'; } }
	else if (key == '\n') { if (m_buf_len < 510) { m_buffer[m_buf_len++]='\n'; m_buffer[m_buf_len]='\0'; } }
	else if (m_buf_len < 510 && (uint8_t)key >= 32 && (uint8_t)key < 127) { m_buffer[m_buf_len++]=key; m_buffer[m_buf_len]='\0'; }
}

void TextEditorWindow::handle_click(int mx, int my) {
	if (in_rect(mx, my, px() + 12, content_y() + 6, 72, 28)) save_file();
}

// ── 4. Clock ─────────────────────────────────────────────────────────────────

ClockWindow::ClockWindow(int x, int y)
	: Window("Clock", x, y, 40, 16) {
	m_body_color = rit::Color::Black;
}

void ClockWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();
	if (!rit::System::fb_is_avail()) return;

	int wx = px(), wy = content_y(), ww = pw();
	int hr=0, mn=0, sc=0; rit::System::get_time(hr, mn, sc);
	int yr=0, mo=0, dy=0; rit::System::get_date(yr, mo, dy);

	/* Big time, centred */
	char ts[9];
	ts[0]=(hr/10)+'0'; ts[1]=(hr%10)+'0'; ts[2]=':';
	ts[3]=(mn/10)+'0'; ts[4]=(mn%10)+'0'; ts[5]=':';
	ts[6]=(sc/10)+'0'; ts[7]=(sc%10)+'0'; ts[8]='\0';
	fluent::text_centered(ts, fluent::TEXT, wx, ww, wy + 36, fluent::FONT_H1);

	/* Date below */
	int fy = 2000 + yr;
	char ds[11];
	ds[0]=(fy/1000)+'0'; ds[1]=((fy/100)%10)+'0'; ds[2]=((fy/10)%10)+'0'; ds[3]=(fy%10)+'0';
	ds[4]='-'; ds[5]=(mo/10)+'0'; ds[6]=(mo%10)+'0';
	ds[7]='-'; ds[8]=(dy/10)+'0'; ds[9]=(dy%10)+'0'; ds[10]='\0';
	fluent::text_centered(ds, fluent::TEXT_SEC, wx, ww, wy + 100);

	/* Seconds progress */
	int bar_w = ww - 80;
	int frac  = sc * bar_w / 60;
	fluent::rrect(wx + 40, wy + 132, bar_w, 8, 4, fluent::STROKE);
	if (frac > 4) fluent::rrect(wx + 40, wy + 132, frac, 8, 4, fluent::ACCENT);
}

// ── 5. Calendar ───────────────────────────────────────────────────────────────

CalendarWindow::CalendarWindow(int x, int y)
	: Window("Calendar", x, y, 38, 22) {
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
	if (!rit::System::fb_is_avail()) return;

	int wx = px(), wy = content_y(), ww = pw();
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

	fluent::text(hd, fluent::TEXT, wx + 20, wy + 14);

	/* Grid geometry */
	int gx = wx + 16, gy = wy + 44;
	int cw = (ww - 32) / 7;

	const char* wdays[7] = { "Su","Mo","Tu","We","Th","Fr","Sa" };
	for (int i = 0; i < 7; i++)
		fluent::text_centered(wdays[i], fluent::TEXT_SEC, gx + i*cw, cw, gy,
		                      fluent::FONT_SMALL);

	rit::System::fb_draw_hline_px(gx, gy + 22, ww - 32, fluent::STROKE);

	int start_day = day_of_week(fy, mo, 1);
	int dim = get_days_in_month(fy, mo);
	int grid_row = 0, grid_col = start_day;
	int row_h = 34;

	for (int day = 1; day <= dim; day++) {
		int cx = gx + grid_col*cw, cy = gy + 32 + grid_row*row_h;
		char dbuf[4]; int_to_str(day, dbuf);
		int tx = cx + (cw - fluent::text_w(dbuf)) / 2, ty = cy + 7;

		if (day == dy) {
			/* Win11 today: accent filled circle */
			rit::System::fb_fill_circle(cx + cw/2, cy + 15, 14, fluent::ACCENT);
			fluent::text(dbuf, fluent::TEXT_WHITE, tx, ty);
		} else {
			uint32_t fg = (grid_col==0||grid_col==6) ? fluent::TEXT_SEC : fluent::TEXT;
			fluent::text(dbuf, fg, tx, ty);
		}
		grid_col++;
		if (grid_col >= 7) { grid_col=0; grid_row++; }
	}
}

// ── 6. Settings ───────────────────────────────────────────────────────────────

#define SET_NAV_W 176

static const char* const k_set_pages[3] = { "Personalization", "System", "About" };

SettingsWindow::SettingsWindow(int x, int y)
	: Window("Settings", x, y, 70, 28), m_page(0) {
	m_body_color = rit::Color::Black;
}

void SettingsWindow::draw() {
	if (!m_visible || m_minimized) return;
	Window::draw();
	if (!rit::System::fb_is_avail()) return;

	int wx = px(), wy = content_y(), ww = pw(), wh = content_h();

	/* Nav sidebar */
	fluent::rect(wx + 1, wy, SET_NAV_W, wh - 1, fluent::MICA);
	for (int i = 0; i < 3; i++) {
		int iy = wy + 12 + i * 40;
		if (i == m_page) {
			fluent::rrect(wx + 8, iy, SET_NAV_W - 16, 34, 5, fluent::CARD);
			fluent::rrect(wx + 8, iy + 9, 3, 16, 1, fluent::ACCENT);
		}
		fluent::text(k_set_pages[i], i == m_page ? fluent::TEXT : fluent::TEXT_SEC,
		             wx + 24, iy + 9);
	}

	int cx = wx + SET_NAV_W + 24, cy = wy + 16;
	int cw = ww - SET_NAV_W - 48;

	fluent::text(k_set_pages[m_page], fluent::TEXT, cx, cy);
	cy += 32;

	if (m_page == 0) {
		/* Personalization: background mode */
		char mode = 'W';
		if (rit::VFS::exists("/users/ramim/configuration/appearance.txt")) {
			const char* cfg = rit::VFS::read_file("/users/ramim/configuration/appearance.txt");
			if (cfg && cfg[0]) mode = cfg[0];
		}
		card(cx, cy, cw, 110);
		fluent::text("Background", fluent::TEXT, cx + 16, cy + 12);
		fluent::text("Choose what shows on your desktop",
		             fluent::TEXT_SEC, cx + 16, cy + 32);
		fluent::button(cx + 16,  cy + 62, 130, 32, "Wallpaper",   false, mode != 'S');
		fluent::button(cx + 156, cy + 62, 130, 32, "Solid color", false, mode == 'S');
	} else if (m_page == 1) {
		/* System: power actions + memory */
		card(cx, cy, cw, 96);
		fluent::text("Power", fluent::TEXT, cx + 16, cy + 12);
		fluent::button(cx + 16,  cy + 44, 130, 32, "Shut down", false, false);
		fluent::button(cx + 156, cy + 44, 130, 32, "Restart",   false, false);

		card(cx, cy + 108, cw, 64);
		fluent::text("Memory in use", fluent::TEXT, cx + 16, cy + 120);
		size_t heap = rit::System::get_heap_usage();
		char hbuf[16]; int_to_str((int)(heap / 1024), hbuf);
		int hl = 0; while (hbuf[hl]) hl++;
		hbuf[hl]=' '; hbuf[hl+1]='K'; hbuf[hl+2]='B'; hbuf[hl+3]='\0';
		fluent::text(hbuf, fluent::TEXT_SEC, cx + 16, cy + 140);
	} else {
		card(cx, cy, cw, 130);
		fluent::text("RitOS", fluent::TEXT, cx + 16, cy + 14);
		fluent::text("Version 0.4.0", fluent::TEXT_SEC, cx + 16, cy + 38);
		fluent::text("A hobby operating system for x86", fluent::TEXT_SEC, cx + 16, cy + 62);
		fluent::text("Fluent UI shell over a RAM filesystem", fluent::TEXT_SEC, cx + 16, cy + 86);
	}
}

void SettingsWindow::handle_click(int mx, int my) {
	int wx = px(), wy = content_y(), ww = pw();

	/* Nav */
	if (mx < wx + SET_NAV_W) {
		for (int i = 0; i < 3; i++)
			if (in_rect(mx, my, wx + 8, wy + 12 + i * 40, SET_NAV_W - 16, 34)) { m_page = i; return; }
		return;
	}

	int cx = wx + SET_NAV_W + 24, cy = wy + 48;
	(void)ww;

	if (m_page == 0) {
		const char* cfgp = "/users/ramim/configuration/appearance.txt";
		if (in_rect(mx, my, cx + 16, cy + 62, 130, 32)) {
			if (rit::VFS::exists(cfgp)) rit::VFS::write_file(cfgp, "W");
			else rit::VFS::create_file(cfgp, "W");
		} else if (in_rect(mx, my, cx + 156, cy + 62, 130, 32)) {
			if (rit::VFS::exists(cfgp)) rit::VFS::write_file(cfgp, "S");
			else rit::VFS::create_file(cfgp, "S");
		}
	} else if (m_page == 1) {
		if (in_rect(mx, my, cx + 16, cy + 44, 130, 32))  rit::System::shutdown();
		if (in_rect(mx, my, cx + 156, cy + 44, 130, 32)) rit::System::reboot();
	}
}

} // namespace ritos

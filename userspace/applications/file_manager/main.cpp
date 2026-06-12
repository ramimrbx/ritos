#include <ritos/window.hpp>
#include <ritos/fluent.hpp>
#include <rit/system.hpp>
#include <rit/virtual_filesystem.hpp>
#include <rit/rbx_module.h>

extern const RitOS_API* g_api;

/*
 * File Explorer — Windows-11-style, full featured:
 *   sidebar (Home/Desktop/Documents/Launcher/System/Volumes/Temporary),
 *   toolbar (New file / New folder / Cut / Copy / Paste / Rename / Delete),
 *   back-forward-up + breadcrumb address bar, search filter,
 *   Details list (Name / Type / Size) with folders-first virtual
 *   directories derived from path prefixes, scrollbar and status bar.
 *
 * Open rules: .rbx launches it, .stct launches its target, anything else
 * goes to the Text Editor via /temporary/open_file.txt. Clicking the
 * already-selected row opens it.
 */

static const Desktop_Interface* g_fm_desktop = nullptr;

/* ── small string helpers ─────────────────────────────────────────────── */
static int  slen(const char* s) { int n=0; while (s[n]) n++; return n; }
static void scpy(char* d, const char* s, int max) {
	int i=0; while (s[i] && i < max-1) { d[i]=s[i]; i++; } d[i]='\0';
}
static bool seq(const char* a, const char* b) {
	int i=0; while (a[i]&&b[i]) { if (a[i]!=b[i]) return false; i++; } return a[i]==b[i];
}
static bool sprefix(const char* s, const char* pre) {
	int i=0; while (pre[i]) { if (s[i]!=pre[i]) return false; i++; } return true;
}
static bool ssub(const char* hay, const char* needle) {
	int nl = slen(needle);
	if (nl == 0) return true;
	int hl = slen(hay);
	for (int s = 0; s + nl <= hl; s++) {
		int k = 0;
		char a, b;
		while (k < nl) {
			a = hay[s+k]; b = needle[k];
			if (a>='A'&&a<='Z') a += 32;
			if (b>='A'&&b<='Z') b += 32;
			if (a != b) break;
			k++;
		}
		if (k == nl) return true;
	}
	return false;
}
static void int_str(int n, char* buf) {
	int i=0; if (n==0) buf[i++]='0';
	while (n>0) { buf[i++]=(n%10)+'0'; n/=10; }
	buf[i]='\0';
	for (int j=0;j<i/2;j++){char t=buf[j];buf[j]=buf[i-j-1];buf[i-j-1]=t;}
}
static bool ext_is(const char* f, const char* ext) { /* ext like ".rbx" */
	int fl = slen(f), el = slen(ext);
	if (fl <= el) return false;
	for (int i = 0; i < el; i++) if (f[fl-el+i] != ext[i]) return false;
	return true;
}

/* ── layout ───────────────────────────────────────────────────────────── */
#define TB_H    44   /* toolbar          */
#define AB_H    38   /* address bar      */
#define SIDE_W  170  /* sidebar          */
#define HDR_H   26   /* column header    */
#define ROW_H   28   /* list row         */
#define STAT_H  26   /* status bar       */
#define SCROLL_W 10

#define MAX_ENTRIES 64
#define MAX_HIST    8

struct Entry {
	char name[44];
	char full[64];
	bool is_dir;
	int  size;
};

struct SideItem { const char* label; const char* path; };
static const SideItem k_side[7] = {
	{ "Home",      "/" },
	{ "Desktop",   "/users/ramim/desktop/" },
	{ "Documents", "/users/ramim/documents/" },
	{ "Launcher",  "/users/ramim/launcher/" },
	{ "System",    "/system/" },
	{ "This PC",   "/volumes/" },
	{ "Temporary", "/temporary/" },
};

/* Files under this prefix are a live mirror of the physical disk's FAT
 * root directory: every create/write/rename/delete is replayed on disk. */
#define DISK_PREFIX "/volumes/disk/"

static bool path_on_disk(const char* p) { return sprefix(p, DISK_PREFIX); }
static const char* disk_basename(const char* p) { return p + slen(DISK_PREFIX); }

class FileExplorerWindow : public ritos::Window {
	char  m_cwd[64];
	Entry m_entries[MAX_ENTRIES];
	int   m_count;
	int   m_selected;
	int   m_scroll;

	char  m_search[24];
	int   m_search_len;
	bool  m_search_focus;

	char  m_clip[64];
	bool  m_clip_cut;

	bool  m_rename_mode;
	char  m_rename[40];
	int   m_rename_len;

	char  m_hist[MAX_HIST][64];
	int   m_hist_pos;   /* index of current entry */
	int   m_hist_len;

public:
	FileExplorerWindow(int x, int y)
		: ritos::Window("File Explorer", x, y, 112, 40),
		  m_count(0), m_selected(-1), m_scroll(0),
		  m_search_len(0), m_search_focus(false), m_clip_cut(false),
		  m_rename_mode(false), m_rename_len(0), m_hist_pos(0), m_hist_len(1) {
		m_body_color = rit::Color::Black;
		scpy(m_cwd, "/", 64);
		scpy(m_hist[0], "/", 64);
		m_search[0]='\0'; m_clip[0]='\0'; m_rename[0]='\0';
		refresh();
	}

	/* ── model ────────────────────────────────────────────────────────── */
	void refresh() {
		m_count = 0;
		const char* files[64];
		int n = rit::VFS::get_file_list(files, 64);
		int cl = slen(m_cwd);

		/* pass 1: virtual folders (first path segment below cwd) */
		for (int i = 0; i < n && m_count < MAX_ENTRIES; i++) {
			const char* f = files[i];
			if (!sprefix(f, m_cwd)) continue;
			const char* rest = f + cl;
			int sl2 = -1;
			for (int k = 0; rest[k]; k++) if (rest[k] == '/') { sl2 = k; break; }
			if (sl2 <= 0) continue;
			char dname[44];
			int dl = sl2 < 43 ? sl2 : 43;
			for (int k = 0; k < dl; k++) dname[k] = rest[k];
			dname[dl] = '\0';
			if (!ssub(dname, m_search)) continue;
			bool dup = false;
			for (int e = 0; e < m_count; e++)
				if (m_entries[e].is_dir && seq(m_entries[e].name, dname)) { dup = true; break; }
			if (dup) continue;
			Entry& en = m_entries[m_count++];
			scpy(en.name, dname, 44);
			scpy(en.full, m_cwd, 64);
			int fl = slen(en.full);
			for (int k = 0; dname[k] && fl < 62; k++) en.full[fl++] = dname[k];
			en.full[fl++] = '/'; en.full[fl] = '\0';
			en.is_dir = true; en.size = 0;
		}
		/* pass 2: files directly in cwd */
		for (int i = 0; i < n && m_count < MAX_ENTRIES; i++) {
			const char* f = files[i];
			if (!sprefix(f, m_cwd)) continue;
			const char* rest = f + cl;
			bool has_slash = false;
			for (int k = 0; rest[k]; k++) if (rest[k] == '/') { has_slash = true; break; }
			if (has_slash || !rest[0]) continue;
			if (!ssub(rest, m_search)) continue;
			Entry& en = m_entries[m_count++];
			scpy(en.name, rest, 44);
			scpy(en.full, f, 64);
			en.is_dir = false;
			en.size = 0;
			rit::VFS::read_file(f, &en.size);
		}
		if (m_selected >= m_count) m_selected = m_count - 1;
		if (m_scroll > m_count) m_scroll = 0;
	}

	void navigate(const char* path, bool push_history) {
		scpy(m_cwd, path, 64);
		m_selected = -1; m_scroll = 0;
		m_search_len = 0; m_search[0]='\0'; m_search_focus = false;
		if (push_history) {
			if (m_hist_pos < MAX_HIST - 1) {
				m_hist_pos++;
				scpy(m_hist[m_hist_pos], path, 64);
				m_hist_len = m_hist_pos + 1;
			} else {
				for (int i = 0; i < MAX_HIST - 1; i++) scpy(m_hist[i], m_hist[i+1], 64);
				scpy(m_hist[MAX_HIST-1], path, 64);
			}
		}
		refresh();
	}

	void go_back()    { if (m_hist_pos > 0) { m_hist_pos--; navigate(m_hist[m_hist_pos], false); } }
	void go_forward() { if (m_hist_pos < m_hist_len - 1) { m_hist_pos++; navigate(m_hist[m_hist_pos], false); } }
	void go_up() {
		int cl = slen(m_cwd);
		if (cl <= 1) return;
		char up[64]; scpy(up, m_cwd, 64);
		up[cl-1] = '\0';                       /* drop trailing slash  */
		int k = slen(up);
		while (k > 0 && up[k-1] != '/') k--;   /* drop last segment    */
		up[k] = '\0';
		if (k == 0) scpy(up, "/", 64);
		navigate(up, true);
	}

	void open_entry(int idx) {
		if (idx < 0 || idx >= m_count) return;
		Entry& e = m_entries[idx];
		if (e.is_dir) { navigate(e.full, true); return; }
		if (ext_is(e.full, ".rbx")) {
			if (g_fm_desktop) g_fm_desktop->launch_app(e.full);
		} else if (ext_is(e.full, ".stct")) {
			const char* t = rit::VFS::read_file(e.full);
			if (t && g_fm_desktop) g_fm_desktop->launch_app(t);
		} else {
			rit::VFS::create_file("/temporary/open_file.txt", e.full);
			if (g_fm_desktop) g_fm_desktop->launch_app("Text Editor");
		}
	}

	void make_unique(char* out, const char* base, const char* ext) {
		for (int num = 1; num < 100; num++) {
			int l = 0;
			while (m_cwd[l]) { out[l] = m_cwd[l]; l++; }
			for (int k = 0; base[k] && l < 56; k++) out[l++] = base[k];
			char nb[8]; int_str(num, nb);
			for (int k = 0; nb[k]; k++) out[l++] = nb[k];
			for (int k = 0; ext[k]; k++) out[l++] = ext[k];
			out[l] = '\0';
			if (!rit::VFS::exists(out)) return;
		}
	}

	void new_file() {
		/* FAT 8.3 names: keep the base short when creating on the disk */
		char nn[64]; make_unique(nn, path_on_disk(m_cwd) ? "file" : "new_file", ".txt");
		rit::VFS::create_file(nn, " ");
		if (path_on_disk(nn)) g_api->storage_export(nn);
		refresh();
	}
	void new_folder() {
		/* flat VFS: a folder exists as a path prefix, so seed it with a file */
		char base[64]; make_unique(base, "new_folder", "");
		char nn[64]; scpy(nn, base, 64);
		int l = slen(nn);
		const char* seed = "/about.txt";
		for (int k = 0; seed[k] && l < 63; k++) nn[l++] = seed[k];
		nn[l] = '\0';
		rit::VFS::create_file(nn, "New folder");
		refresh();
	}
	void clip_set(bool cut) {
		if (m_selected < 0 || m_selected >= m_count) return;
		if (m_entries[m_selected].is_dir) return;   /* files only */
		scpy(m_clip, m_entries[m_selected].full, 64);
		m_clip_cut = cut;
	}
	void clip_paste() {
		if (!m_clip[0] || !rit::VFS::exists(m_clip)) return;
		int len = 0;
		const char* data = rit::VFS::read_file(m_clip, &len);
		if (!data) return;
		/* destination: cwd + source basename (made unique if taken) */
		int bl = slen(m_clip);
		int bs = bl; while (bs > 0 && m_clip[bs-1] != '/') bs--;
		char dest[64]; scpy(dest, m_cwd, 64);
		int dl = slen(dest);
		for (int k = bs; m_clip[k] && dl < 63; k++) dest[dl++] = m_clip[k];
		dest[dl] = '\0';
		if (rit::VFS::exists(dest)) {
			char alt[64]; scpy(alt, m_cwd, 64);
			int al = slen(alt);
			const char* pre = "copy_";
			for (int k = 0; pre[k]; k++) alt[al++] = pre[k];
			for (int k = bs; m_clip[k] && al < 63; k++) alt[al++] = m_clip[k];
			alt[al] = '\0';
			scpy(dest, alt, 64);
		}
		g_api->vfs_write_file(dest, data, len);
		if (path_on_disk(dest)) g_api->storage_export(dest);
		if (m_clip_cut) {
			if (path_on_disk(m_clip)) g_api->storage_delete(disk_basename(m_clip));
			rit::VFS::delete_file(m_clip);
			m_clip[0]='\0';
		}
		refresh();
	}
	void delete_selected() {
		if (m_selected < 0 || m_selected >= m_count) return;
		Entry& e = m_entries[m_selected];
		if (e.is_dir) {
			/* delete everything under the prefix */
			const char* files[64];
			int n = rit::VFS::get_file_list(files, 64);
			char pre[64]; scpy(pre, e.full, 64);
			/* collect first: deleting mutates the list */
			char doomed[16][64]; int dn = 0;
			for (int i = 0; i < n && dn < 16; i++)
				if (sprefix(files[i], pre)) scpy(doomed[dn++], files[i], 64);
			for (int i = 0; i < dn; i++) {
				if (path_on_disk(doomed[i])) g_api->storage_delete(disk_basename(doomed[i]));
				rit::VFS::delete_file(doomed[i]);
			}
		} else {
			if (path_on_disk(e.full)) g_api->storage_delete(disk_basename(e.full));
			rit::VFS::delete_file(e.full);
		}
		m_selected = -1;
		refresh();
	}
	void start_rename() {
		if (m_selected < 0 || m_selected >= m_count) return;
		if (m_entries[m_selected].is_dir) return;   /* files only */
		scpy(m_rename, m_entries[m_selected].name, 40);
		m_rename_len = slen(m_rename);
		m_rename_mode = true;
	}
	void commit_rename() {
		if (m_selected < 0 || m_selected >= m_count || m_rename_len == 0) {
			m_rename_mode = false; return;
		}
		char np[64]; scpy(np, m_cwd, 64);
		int l = slen(np);
		for (int k = 0; m_rename[k] && l < 63; k++) np[l++] = m_rename[k];
		np[l] = '\0';
		if (path_on_disk(m_entries[m_selected].full))
			g_api->storage_rename(disk_basename(m_entries[m_selected].full),
			                      disk_basename(np));
		rit::VFS::rename_file(m_entries[m_selected].full, np);
		m_rename_mode = false;
		refresh();
	}

	/* ── geometry ─────────────────────────────────────────────────────── */
	int list_x() const { return px() + SIDE_W + 1; }
	int list_y() const { return content_y() + TB_H + AB_H + HDR_H; }
	int list_w() const { return pw() - SIDE_W - 2 - SCROLL_W; }
	int list_h() const { return content_h() - TB_H - AB_H - HDR_H - STAT_H; }
	/* "This PC" shows a drive panel above the file rows */
	int drive_card_h() const { return seq(m_cwd, "/volumes/") ? 104 : 0; }
	int rows_visible() const {
		int r = (list_h() - drive_card_h()) / ROW_H;
		return r < 1 ? 1 : r;
	}

	/* ── drawing ──────────────────────────────────────────────────────── */
	void draw() override {
		if (!m_visible || m_minimized) return;
		ritos::Window::draw();
		if (!rit::System::fb_is_avail()) return;

		int wx = px(), wy = content_y(), ww = pw();
		int mx = g_fm_desktop ? g_fm_desktop->get_mouse_px_x() : -1;
		int my = g_fm_desktop ? g_fm_desktop->get_mouse_px_y() : -1;

		draw_toolbar(wx, wy, ww, mx, my);
		draw_addressbar(wx, wy + TB_H, ww, mx, my);
		draw_sidebar(wx, wy + TB_H + AB_H, mx, my);
		draw_list(mx, my);
		draw_statusbar(wx, ww);
		if (m_rename_mode) draw_rename_modal();
	}

	void draw_toolbar(int wx, int wy, int ww, int mx, int my) {
		fluent::rect(wx + 1, wy, ww - 2, TB_H, fluent::MICA);
		rit::System::fb_draw_hline_px(wx + 1, wy + TB_H - 1, ww - 2, fluent::STROKE);

		int x = wx + 12, y = wy + 8, h = 28;
		bool hov;

		hov = in(mx, my, x, y, 88, h);
		fluent::button(x, y, 88, h, "New file", hov, false);
		x += 96;
		hov = in(mx, my, x, y, 104, h);
		fluent::button(x, y, 104, h, "New folder", hov, false);
		x += 112;

		fluent::rect(x, y + 4, 1, h - 8, fluent::BORDER_DIM);  /* separator */
		x += 9;

		struct { const char* tip; int id; } acts[5] = {
			{"Cut", 0}, {"Copy", 1}, {"Paste", 2}, {"Rename", 3}, {"Delete", 4}
		};
		for (int i = 0; i < 5; i++) {
			hov = in(mx, my, x, y, 34, h);
			if (hov) fluent::rrect(x, y, 34, h, 5, fluent::HOVER);
			uint32_t bg = hov ? fluent::HOVER : fluent::MICA;
			int gx = x + 10, gy = y + 7;
			switch (acts[i].id) {
				case 0: fluent::glyph_cut(gx, gy, 14, fluent::TEXT_SEC);   break;
				case 1: fluent::glyph_copy(gx, gy, 14, fluent::TEXT_SEC);  break;
				case 2: fluent::glyph_paste(gx, gy, 14, fluent::TEXT_SEC); break;
				case 3: fluent::glyph_pen(gx, gy, 14, fluent::TEXT_SEC);   break;
				case 4: fluent::glyph_trash(gx, gy, 14, fluent::TEXT_SEC); break;
			}
			(void)bg;
			x += 40;
		}
	}

	void draw_addressbar(int wx, int wy, int ww, int mx, int my) {
		fluent::rect(wx + 1, wy, ww - 2, AB_H, fluent::MICA);
		rit::System::fb_draw_hline_px(wx + 1, wy + AB_H - 1, ww - 2, fluent::STROKE);

		int y = wy + 5, h = 28;
		int x = wx + 10;
		/* back / forward / up / refresh */
		bool can_back = m_hist_pos > 0, can_fwd = m_hist_pos < m_hist_len - 1;
		if (in(mx, my, x, y, 28, h) && can_back) fluent::rrect(x, y, 28, h, 4, fluent::HOVER);
		fluent::chevron_left(x + 10, y + 8, 12, can_back ? fluent::TEXT : fluent::TEXT_DIS);
		x += 32;
		if (in(mx, my, x, y, 28, h) && can_fwd) fluent::rrect(x, y, 28, h, 4, fluent::HOVER);
		fluent::chevron_right(x + 8, y + 8, 12, can_fwd ? fluent::TEXT : fluent::TEXT_DIS);
		x += 32;
		if (in(mx, my, x, y, 28, h)) fluent::rrect(x, y, 28, h, 4, fluent::HOVER);
		fluent::arrow_up(x + 8, y + 6, 12, slen(m_cwd) > 1 ? fluent::TEXT : fluent::TEXT_DIS);
		x += 32;
		if (in(mx, my, x, y, 28, h)) fluent::rrect(x, y, 28, h, 4, fluent::HOVER);
		fluent::glyph_refresh(x + 7, y + 7, 14, fluent::TEXT_SEC,
		                      in(mx, my, x, y, 28, h) ? fluent::HOVER : fluent::MICA);
		x += 36;

		/* breadcrumb box */
		int search_w = 168;
		int bb_w = wx + ww - 12 - search_w - 8 - x;
		fluent::rrect_border(x, y, bb_w, h, 5, fluent::BORDER_DIM, fluent::CARD);
		int cx2 = x + 10;
		cx2 += draw_crumb("RitOS", cx2, y, mx, my);
		/* path segments */
		int cl = slen(m_cwd);
		int seg_start = 1;
		for (int i = 1; i <= cl; i++) {
			if (i == cl || m_cwd[i] == '/') {
				if (i > seg_start) {
					fluent::chevron_right(cx2 + 2, y + 9, 10, fluent::TEXT_DIS);
					cx2 += 14;
					char seg[44];
					int sl2 = i - seg_start; if (sl2 > 43) sl2 = 43;
					for (int k = 0; k < sl2; k++) seg[k] = m_cwd[seg_start + k];
					seg[sl2] = '\0';
					if (cx2 + fluent::text_w(seg) < x + bb_w - 8)
						cx2 += draw_crumb(seg, cx2, y, mx, my);
				}
				seg_start = i + 1;
			}
		}

		/* search box */
		int sx = wx + ww - 12 - search_w;
		fluent::rrect_border(sx, y, search_w, h, 5,
		                     m_search_focus ? fluent::ACCENT : fluent::BORDER_DIM,
		                     fluent::CARD);
		fluent::glyph_search(sx + 8, y + 8, 12, fluent::TEXT_SEC, fluent::CARD);
		if (m_search_len > 0) {
			fluent::text(m_search, fluent::TEXT, sx + 26, y + 6);
			if (m_search_focus)
				fluent::rect(sx + 27 + fluent::text_w(m_search), y + 5, 1, 18, fluent::TEXT);
		} else {
			fluent::text("Search", fluent::TEXT_DIS, sx + 26, y + 6);
			if (m_search_focus)
				fluent::rect(sx + 26, y + 5, 1, 18, fluent::TEXT);
		}
	}

	int draw_crumb(const char* s, int x, int y, int mx, int my) {
		int w = fluent::text_w(s) + 8;
		if (in(mx, my, x - 4, y + 3, w, 22))
			fluent::rrect(x - 4, y + 3, w, 22, 3, fluent::HOVER);
		fluent::text(s, fluent::TEXT, x, y + 6);
		return w + 2;
	}

	void draw_sidebar(int wx, int wy, int mx, int my) {
		int h = content_h() - TB_H - AB_H;
		fluent::rect(wx + 1, wy, SIDE_W, h, fluent::MICA);
		fluent::rect(wx + SIDE_W, wy, 1, h, fluent::STROKE);

		for (int i = 0; i < 7; i++) {
			int iy = wy + 10 + i * 36;
			bool cur = seq(m_cwd, k_side[i].path);
			bool hov = in(mx, my, wx + 8, iy, SIDE_W - 16, 32);
			if (cur)      fluent::rrect(wx + 8, iy, SIDE_W - 16, 32, 5, fluent::CARD);
			else if (hov) fluent::rrect(wx + 8, iy, SIDE_W - 16, 32, 5, fluent::HOVER);
			if (cur) fluent::rrect(wx + 8, iy + 8, 3, 16, 1, fluent::ACCENT);

			if (i == 0) fluent::glyph_home(wx + 20, iy + 8, 16, fluent::TEXT_SEC);
			else        fluent::glyph_folder(wx + 20, iy + 8, 16);
			fluent::text(k_side[i].label, cur ? fluent::TEXT : fluent::TEXT_SEC,
			             wx + 44, iy + 8);
		}
	}

	/* Detected drive panel: model, capacity, filesystem + a Mount button */
	void draw_drive_card(int lx, int ly, int lw, int mx, int my) {
		int cx = lx + 8, cy = ly + 8, cw = lw - 16, ch = 88;
		fluent::rrect_border(cx, cy, cw, ch, 7, fluent::STROKE, fluent::CARD_ALT);

		/* drive glyph */
		fluent::rrect_border(cx + 14, cy + 28, 40, 26, 4, fluent::TEXT_SEC, fluent::CARD);
		fluent::rect(cx + 44, cy + 46, 5, 4, fluent::ACCENT);

		char info[256];
		int operable = g_api->storage_describe(info, 256);
		(void)operable;
		/* split into lines, draw up to 4 */
		int line = 0, start = 0;
		for (int i = 0; line < 4; i++) {
			if (info[i] == '\n' || info[i] == '\0') {
				char lb[64];
				int n = i - start; if (n > 63) n = 63;
				for (int k = 0; k < n; k++) lb[k] = info[start + k];
				lb[n] = '\0';
				int maxc = (cw - 200) / 8;
				if (n > maxc && maxc > 3) lb[maxc] = '\0';
				fluent::text(lb, line == 0 ? fluent::TEXT : fluent::TEXT_SEC,
				             cx + 68, cy + 10 + line * 19);
				line++;
				start = i + 1;
				if (info[i] == '\0') break;
			}
		}

		bool hov = in(mx, my, cx + cw - 122, cy + 28, 110, 32);
		fluent::button(cx + cw - 122, cy + 28, 110, 32, "Mount", hov, true);
	}

	void draw_list(int mx, int my) {
		int lx = list_x(), ly = list_y(), lw = list_w(), lh = list_h();
		int hy = ly - HDR_H;

		/* column header */
		int col_type = lx + lw - 200;
		int col_size = lx + lw - 96;
		fluent::text("Name", fluent::TEXT_SEC, lx + 40, hy + 5);
		fluent::text("Type", fluent::TEXT_SEC, col_type, hy + 5);
		fluent::text("Size", fluent::TEXT_SEC, col_size, hy + 5);
		rit::System::fb_draw_hline_px(lx, hy + HDR_H - 1, lw + SCROLL_W, fluent::STROKE);

		if (drive_card_h()) {
			draw_drive_card(lx, ly, lw, mx, my);
			ly += drive_card_h();
			lh -= drive_card_h();
		}

		int vis = rows_visible();
		for (int r = 0; r < vis; r++) {
			int idx = m_scroll + r;
			if (idx >= m_count) break;
			Entry& e = m_entries[idx];
			int ry = ly + r * ROW_H;

			bool sel = (idx == m_selected);
			bool hov = in(mx, my, lx, ry, lw, ROW_H);
			if (sel)      fluent::rrect(lx + 2, ry + 1, lw - 4, ROW_H - 2, 4, fluent::ACCENT_SOFT);
			else if (hov) fluent::rrect(lx + 2, ry + 1, lw - 4, ROW_H - 2, 4, fluent::HOVER);

			/* icon */
			if (e.is_dir)                     fluent::glyph_folder(lx + 12, ry + 6, 16);
			else if (ext_is(e.full, ".rbx"))  fluent::glyph_app(lx + 12, ry + 6, 16);
			else                              fluent::glyph_doc(lx + 13, ry + 6, 15);

			/* name (rename inline when editing the selected row) */
			if (m_rename_mode && sel) {
				/* handled by modal */
				fluent::text(e.name, fluent::TEXT_DIS, lx + 40, ry + 6);
			} else {
				int maxc = (col_type - (lx + 40)) / 8 - 1;
				char nm[44]; scpy(nm, e.name, 44);
				if (slen(nm) > maxc && maxc > 3) { nm[maxc-2]='.'; nm[maxc-1]='.'; nm[maxc]='\0'; }
				fluent::text(nm, fluent::TEXT, lx + 40, ry + 6);
			}

			/* type */
			const char* ty = e.is_dir ? "Folder"
			              : ext_is(e.full, ".rbx")  ? "Application"
			              : ext_is(e.full, ".stct") ? "Shortcut"
			              : ext_is(e.full, ".txt")  ? "Text file" : "File";
			fluent::text(ty, fluent::TEXT_SEC, col_type, ry + 6);

			/* size (right aligned, files only) */
			if (!e.is_dir) {
				char sb[16]; int_str(e.size, sb);
				int sl2 = slen(sb);
				sb[sl2]=' '; sb[sl2+1]='B'; sb[sl2+2]='\0';
				fluent::text(sb, fluent::TEXT_SEC,
				             lx + lw - 12 - fluent::text_w(sb), ry + 6);
			}
		}

		if (m_count == 0)
			fluent::text_centered("This folder is empty", fluent::TEXT_DIS,
			                      lx, lw, ly + 24);

		/* scrollbar */
		if (m_count > vis) {
			int track_x = lx + lw + 1;
			fluent::rect(track_x, ly, SCROLL_W - 2, lh, fluent::CARD_ALT);
			int th = lh * vis / m_count; if (th < 24) th = 24;
			int ty = ly + (lh - th) * m_scroll / (m_count - vis);
			fluent::rrect(track_x + 1, tap_clamp(ty, ly, ly + lh - th), SCROLL_W - 4, th, 3, 0xFFB8B8B8u);
		}
	}

	static int tap_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

	void draw_statusbar(int wx, int ww) {
		int sy = py() + ph() - STAT_H;
		fluent::rect(wx + 1, sy, ww - 2, STAT_H - 1, fluent::CARD_ALT);
		rit::System::fb_draw_hline_px(wx + 1, sy, ww - 2, fluent::STROKE);

		char buf[32]; int_str(m_count, buf);
		int l = slen(buf);
		const char* sfx = " items";
		for (int k = 0; sfx[k]; k++) buf[l+k] = sfx[k];
		buf[l + 6] = '\0';
		fluent::text(buf, fluent::TEXT_SEC, wx + 14, sy + 5);

		if (m_selected >= 0 && m_selected < m_count)
			fluent::text("1 item selected", fluent::TEXT_SEC,
			             wx + 30 + fluent::text_w(buf), sy + 5);
		if (m_clip[0])
			fluent::text(m_clip_cut ? "clipboard: cut" : "clipboard: copy",
			             fluent::TEXT_DIS, wx + ww - 140, sy + 5);
	}

	void draw_rename_modal() {
		int mw = 320, mh = 130;
		int x = px() + (pw() - mw) / 2, y = py() + (ph() - mh) / 2;
		fluent::shadow(x, y + 4, mw, mh, 8, 18, 0x50000000u);
		fluent::rrect_border(x, y, mw, mh, 8, fluent::BORDER, fluent::MENU_BG);
		fluent::text("Rename", fluent::TEXT, x + 16, y + 14, fluent::FONT_BOLD);
		fluent::rrect_border(x + 16, y + 42, mw - 32, 30, 5, fluent::ACCENT, fluent::CARD);
		fluent::text(m_rename, fluent::TEXT, x + 24, y + 49);
		fluent::rect(x + 25 + fluent::text_w(m_rename), y + 48, 1, 18, fluent::TEXT);
		bool hov_ok = false, hov_ca = false;   /* drawn flat; hover unimportant in modal */
		fluent::button(x + mw - 196, y + mh - 44, 88, 30, "OK", hov_ok, true);
		fluent::button(x + mw - 100, y + mh - 44, 88, 30, "Cancel", hov_ca, false);
	}

	static bool in(int mx, int my, int x, int y, int w, int h) {
		return mx >= x && mx < x + w && my >= y && my < y + h;
	}

	/* ── input (pixel coordinates) ────────────────────────────────────── */
	void handle_click(int mx, int my) override {
		int wx = px(), wy = content_y(), ww = pw();

		if (m_rename_mode) {
			int mw = 320, mh = 130;
			int x = px() + (pw() - mw) / 2, y = py() + (ph() - mh) / 2;
			if (in(mx, my, x + mw - 196, y + mh - 44, 88, 30)) commit_rename();
			else if (in(mx, my, x + mw - 100, y + mh - 44, 88, 30)) m_rename_mode = false;
			return;
		}

		m_search_focus = false;

		/* toolbar */
		if (my < wy + TB_H) {
			int x = wx + 12, y = wy + 8, h = 28;
			if (in(mx, my, x, y, 88, h))        { new_file(); return; }
			x += 96;
			if (in(mx, my, x, y, 104, h))       { new_folder(); return; }
			x += 112 + 9;
			for (int i = 0; i < 5; i++) {
				if (in(mx, my, x, y, 34, h)) {
					switch (i) {
						case 0: clip_set(true);   break;
						case 1: clip_set(false);  break;
						case 2: clip_paste();     break;
						case 3: start_rename();   break;
						case 4: delete_selected();break;
					}
					return;
				}
				x += 40;
			}
			return;
		}

		/* address bar */
		if (my < wy + TB_H + AB_H) {
			int y = wy + TB_H + 5, h = 28;
			int x = wx + 10;
			if (in(mx, my, x, y, 28, h)) { go_back(); return; }
			x += 32;
			if (in(mx, my, x, y, 28, h)) { go_forward(); return; }
			x += 32;
			if (in(mx, my, x, y, 28, h)) { go_up(); return; }
			x += 32;
			if (in(mx, my, x, y, 28, h)) { refresh(); return; }
			x += 36;

			int search_w = 168;
			int sx = wx + ww - 12 - search_w;
			if (in(mx, my, sx, y, search_w, h)) { m_search_focus = true; return; }

			/* breadcrumbs: root crumb, then each segment (text metrics
			 * must mirror draw_breadcrumbs/draw_crumb exactly) */
			int bb_w = sx - 8 - x;
			int cx2 = x + 10;
			int w0 = fluent::text_w("RitOS") + 8;
			if (in(mx, my, cx2 - 4, y + 3, w0, 22)) { navigate("/", true); return; }
			cx2 += w0 + 2;
			int cl = slen(m_cwd);
			int seg_start = 1;
			for (int i = 1; i <= cl; i++) {
				if (i == cl || m_cwd[i] == '/') {
					if (i > seg_start) {
						cx2 += 14;
						int sl2 = i - seg_start; if (sl2 > 43) sl2 = 43;
						char seg[44];
						for (int k = 0; k < sl2; k++) seg[k] = m_cwd[seg_start + k];
						seg[sl2] = '\0';
						int wseg = fluent::text_w(seg) + 8;
						if (cx2 + fluent::text_w(seg) < x + bb_w - 8) {
							if (in(mx, my, cx2 - 4, y + 3, wseg, 22)) {
								char np[64];
								int nl2 = i + 1; if (nl2 > 63) nl2 = 63;
								for (int k = 0; k < nl2 && k < 63; k++) np[k] = m_cwd[k];
								np[m_cwd[i]=='/' ? i+1 : i] = '\0';
								/* ensure trailing slash */
								int pl = slen(np);
								if (pl == 0 || np[pl-1] != '/') { np[pl]='/'; np[pl+1]='\0'; }
								navigate(np, true);
								return;
							}
							cx2 += wseg + 2;
						}
					}
					seg_start = i + 1;
				}
			}
			return;
		}

		/* sidebar */
		if (mx < wx + SIDE_W) {
			int sy = wy + TB_H + AB_H;
			for (int i = 0; i < 7; i++)
				if (in(mx, my, wx + 8, sy + 10 + i * 36, SIDE_W - 16, 32)) {
					navigate(k_side[i].path, true);
					return;
				}
			return;
		}

		/* status bar: nothing clickable */
		if (my >= py() + ph() - STAT_H) return;

		/* drive panel ("This PC"): Mount button imports the FAT root */
		int lx = list_x(), ly = list_y(), lw = list_w(), lh = list_h();
		if (drive_card_h()) {
			int cx = lx + 8, cy = ly + 8, cw = lw - 16;
			if (in(mx, my, cx + cw - 122, cy + 28, 110, 32)) {
				g_api->storage_import();
				refresh();
				return;
			}
			ly += drive_card_h();
			lh -= drive_card_h();
		}

		/* scrollbar track */
		int vis = rows_visible();
		if (mx >= lx + lw && m_count > vis) {
			int max_scroll = m_count - vis;
			if (my < ly + lh / 2) m_scroll -= vis; else m_scroll += vis;
			if (m_scroll < 0) m_scroll = 0;
			if (m_scroll > max_scroll) m_scroll = max_scroll;
			return;
		}

		/* file list: select, or open when clicking the selected row again */
		if (my >= ly && my < ly + lh) {
			int r = (my - ly) / ROW_H;
			int idx = m_scroll + r;
			if (idx >= 0 && idx < m_count) {
				if (idx == m_selected) open_entry(idx);
				else m_selected = idx;
			} else {
				m_selected = -1;
			}
		}
	}

	void handle_key(char key) override {
		if (m_rename_mode) {
			if (key == '\n') commit_rename();
			else if (key == '\b') { if (m_rename_len > 0) m_rename[--m_rename_len]='\0'; }
			else if ((uint8_t)key >= 32 && (uint8_t)key < 127 && m_rename_len < 38) {
				m_rename[m_rename_len++] = key; m_rename[m_rename_len]='\0';
			}
			return;
		}
		if (m_search_focus) {
			if (key == '\n') { m_search_focus = false; }
			else if (key == '\b') { if (m_search_len > 0) m_search[--m_search_len]='\0'; refresh(); }
			else if ((uint8_t)key >= 32 && (uint8_t)key < 127 && m_search_len < 22) {
				m_search[m_search_len++] = key; m_search[m_search_len]='\0'; refresh();
			}
			return;
		}
		int vis = rows_visible();
		if (key == 0x1E) {        /* up   */
			if (m_selected > 0) m_selected--;
			if (m_selected < m_scroll) m_scroll = m_selected;
		} else if (key == 0x1F) { /* down */
			if (m_selected < m_count - 1) m_selected++;
			if (m_selected >= m_scroll + vis) m_scroll = m_selected - vis + 1;
		} else if (key == '\n') {
			open_entry(m_selected);
		} else if (key == '\b') {
			go_up();
		}
	}
};

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
	g_api = api;
	g_fm_desktop = desktop;
	ritos::Window::set_desktop_interface(desktop);

	static FileExplorerWindow* explorer = nullptr;
	if (!explorer) {
		explorer = new FileExplorerWindow(40, 8);
	}

	static rbx_module mod;
	mod.type = RBX_MODULE_APP_WINDOW;
	const char* nm = "File Explorer";
	int i = 0;
	while (nm[i] && i < 31) { mod.name[i] = nm[i]; i++; }
	mod.name[i] = '\0';
	mod.instance = explorer;

	mod.draw = [](void* inst) { static_cast<FileExplorerWindow*>(inst)->draw(); };
	mod.handle_click = [](void* inst, int mx, int my) { static_cast<FileExplorerWindow*>(inst)->handle_click(mx, my); };
	mod.handle_key = [](void* inst, char key) { static_cast<FileExplorerWindow*>(inst)->handle_key(key); };
	mod.destroy = [](void* inst) { delete static_cast<FileExplorerWindow*>(inst); };

	mod.get_x = [](void* inst) { return static_cast<ritos::Window*>(inst)->get_x(); };
	mod.get_y = [](void* inst) { return static_cast<ritos::Window*>(inst)->get_y(); };
	mod.get_width = [](void* inst) { return static_cast<ritos::Window*>(inst)->get_width(); };
	mod.get_height = [](void* inst) { return static_cast<ritos::Window*>(inst)->get_height(); };
	mod.set_position = [](void* inst, int x, int y) { static_cast<ritos::Window*>(inst)->set_position(x, y); };
	mod.is_visible = [](void* inst) { return static_cast<ritos::Window*>(inst)->is_visible() ? 1 : 0; };
	mod.set_visible = [](void* inst, int visible) { static_cast<ritos::Window*>(inst)->set_visible(visible != 0); };
	mod.is_minimized = [](void* inst) { return static_cast<ritos::Window*>(inst)->is_minimized() ? 1 : 0; };
	mod.set_minimized = [](void* inst, int minimized) { static_cast<ritos::Window*>(inst)->set_minimized(minimized != 0); };
	mod.is_active = [](void* inst) { return static_cast<ritos::Window*>(inst)->is_active() ? 1 : 0; };
	mod.set_active = [](void* inst, int active) { static_cast<ritos::Window*>(inst)->set_active(active != 0); };
	mod.get_title = [](void* inst) { return static_cast<ritos::Window*>(inst)->get_title(); };
	mod.is_maximized = [](void* inst) { return static_cast<ritos::Window*>(inst)->is_maximized() ? 1 : 0; };
	mod.set_maximized = [](void* inst, int maximized) { static_cast<ritos::Window*>(inst)->set_maximized(maximized != 0); };

	return &mod;
}

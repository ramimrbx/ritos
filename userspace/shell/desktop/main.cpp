#include <ritos/window.hpp>
#include <ritos/fluent.hpp>
#include <rit/system.hpp>
#include <rit/rbx_module.h>
#include <rit/rbx_format.h>
#include <stdint.h>

extern const RitOS_API* g_api;

/* Fallback desktop colour when no wallpaper is available / solid mode */
#define C_DESK_SOLID 0xFF0F6CBDu
#define C_LABEL      0xFFFFFFFFu
#define C_LABEL_SH   0x90000000u

/* ── Desktop icon grid (Win11-like: large icons down the left edge) ───── */
#define MAX_SHORTCUTS 8
#define SLOT_W   96
#define SLOT_H   90
#define GRID_X0  16
#define GRID_Y0  16
#define ICON_PX  48   /* embedded 32x32 icons drawn scaled to 48x48 */

struct Shortcut {
    char label[24];
    char target[64];
    const uint32_t* icon;   /* points at the icon inside the .rbx in VFS */
};
static Shortcut g_shortcuts[MAX_SHORTCUTS];
static int g_shortcut_count = 0;
static int g_icon_rows = 4;

static int slot_px(int idx) { return GRID_X0 + (idx / g_icon_rows) * SLOT_W; }
static int slot_py(int idx) { return GRID_Y0 + (idx % g_icon_rows) * SLOT_H; }

/* Read the embedded icon straight out of a .rbx file in the VFS */
static const uint32_t* rbx_icon(const char* path) {
    int len = 0;
    const char* data = g_api->vfs_read_file(path, &len);
    if (!data || len < (int)sizeof(rbx_header_t)) return nullptr;
    const rbx_header_t* h = (const rbx_header_t*)data;
    if (h->magic[0]!='R' || h->magic[1]!='B' || h->magic[2]!='X' || h->magic[3]!='2')
        return nullptr;
    if (h->icon_size != RBX_ICON_BYTES) return nullptr;
    if (h->icon_offset + h->icon_size > (uint32_t)len) return nullptr;
    return (const uint32_t*)(data + h->icon_offset);
}

/* Populate the desktop from /desktop/*.stct shortcut files */
static void load_shortcuts() {
    g_shortcut_count = 0;
    const char* files[64];
    int n = g_api->vfs_get_file_list(files, 64);
    for (int i = 0; i < n && g_shortcut_count < MAX_SHORTCUTS; i++) {
        const char* f = files[i];
        const char* pre = "/users/ramim/desktop/";
        int p = 0; while (pre[p] && f[p] == pre[p]) p++;
        if (pre[p]) continue;
        int fl = 0; while (f[fl]) fl++;
        if (fl < 6 || f[fl-5]!='.' || f[fl-4]!='s' || f[fl-3]!='t' || f[fl-2]!='c' || f[fl-1]!='t')
            continue;

        Shortcut& sc = g_shortcuts[g_shortcut_count];
        int li = 0;
        for (int k = p; k < fl - 5 && li < 23; k++) sc.label[li++] = (f[k] == '_') ? ' ' : f[k];
        sc.label[li] = '\0';

        int clen = 0;
        const char* c = g_api->vfs_read_file(f, &clen);
        if (!c || clen <= 0) continue;
        int ti = 0;
        while (ti < clen && ti < 63 && c[ti] && c[ti] != '\n' && c[ti] != '\r') {
            sc.target[ti] = c[ti]; ti++;
        }
        sc.target[ti] = '\0';

        sc.icon = rbx_icon(sc.target);
        g_shortcut_count++;
    }
}

/* ── Windows-style arrow cursor (12×20, hotspot at top-left tip) ──────── */
#define CUR_W 12
#define CUR_H 20
/* X = black outline, # = white fill, . = transparent */
static const char* const g_cursor_map[CUR_H] = {
    "X...........",
    "XX..........",
    "X#X.........",
    "X##X........",
    "X###X.......",
    "X####X......",
    "X#####X.....",
    "X######X....",
    "X#######X...",
    "X########X..",
    "X#########X.",
    "X######XXXXX",
    "X###X##X....",
    "X##X.X##X...",
    "X#X..X##X...",
    "XX....X##X..",
    "......X##X..",
    ".......X##X.",
    ".......X##X.",
    "........XX..",
};
static uint32_t g_cursor_px[CUR_W * CUR_H];
static bool     g_cursor_built = false;

static void build_cursor() {
    for (int y = 0; y < CUR_H; y++)
        for (int x = 0; x < CUR_W; x++) {
            char c = g_cursor_map[y][x];
            g_cursor_px[y * CUR_W + x] =
                (c == 'X') ? 0xFF000000u :
                (c == '#') ? 0xFFFFFFFFu : 0x00000000u;
        }
    g_cursor_built = true;
}

static bool str_equals(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) { if (a[i]!=b[i]) return false; i++; }
    return a[i]==b[i];
}

/* Start-menu panel geometry. The start_menu module computes the exact same
 * values from the framebuffer size; keep the two in sync. */
static int sm_w() { return 560; }
static int sm_h() {
    int h = 620;
    int avail = g_api->fb_get_height() - fluent::TASKBAR_H - 16;
    return h > avail ? avail : h;
}
static int sm_x() { return 8; }
static int sm_y() { return g_api->fb_get_height() - fluent::TASKBAR_H - 8 - sm_h(); }

/* ── RbxWindow ────────────────────────────────────────────────────────── */
class RbxWindow : public ritos::Window {
    rbx_module* m_mod;
    char        m_rbx_path[64];
    const uint32_t* m_icon;
public:
    RbxWindow(rbx_module* mod, const char* path)
        : ritos::Window(mod->get_title(mod->instance),
                        mod->get_x(mod->instance), mod->get_y(mod->instance),
                        mod->get_width(mod->instance), mod->get_height(mod->instance)),
          m_mod(mod) {
        m_visible   = mod->is_visible(mod->instance)   != 0;
        m_minimized = mod->is_minimized(mod->instance) != 0;
        m_active    = mod->is_active(mod->instance)    != 0;
        int i = 0;
        while (path[i] && i < 63) { m_rbx_path[i] = path[i]; i++; }
        m_rbx_path[i] = '\0';
        m_icon = rbx_icon(m_rbx_path);
    }
    const char* get_rbx_path() const { return m_rbx_path; }
    rbx_module* get_module()   const { return m_mod; }
    const uint32_t* get_icon() const { return m_icon; }

    void draw() override {
        if (!m_visible || m_minimized) return;
        ritos::Window::draw();
        m_mod->draw(m_mod->instance);
    }
    void handle_click(int mx, int my) override { m_mod->handle_click(m_mod->instance, mx, my); }
    void handle_key(char key)         override { m_mod->handle_key(m_mod->instance, key); }
    void set_position(int x, int y)   override {
        ritos::Window::set_position(x, y);
        m_mod->set_position(m_mod->instance, x, y);
    }
    void set_visible(bool v)   override { ritos::Window::set_visible(v);   m_mod->set_visible(m_mod->instance, v?1:0); }
    void set_minimized(bool v) override { ritos::Window::set_minimized(v); m_mod->set_minimized(m_mod->instance, v?1:0); }
    void set_active(bool v)    override { ritos::Window::set_active(v);    m_mod->set_active(m_mod->instance, v?1:0); }
    void set_maximized(bool v) override {
        /* Module first, so it snapshots/restores its own original geometry
         * before the wrapper's set_position forwarding overwrites it. */
        if (m_mod->set_maximized) m_mod->set_maximized(m_mod->instance, v?1:0);
        ritos::Window::set_maximized(v);
    }
};

/* ── Desktop ──────────────────────────────────────────────────────────── */
class Desktop {
    ritos::Window* m_windows[16];
    int            m_window_count;
    int            m_mouse_x, m_mouse_y;    /* char cells (legacy interface) */
    int            m_mouse_px, m_mouse_py;  /* exact pixels                  */
    uint8_t        m_mouse_buttons;
    ritos::Window* m_dragged_window;
    int            m_drag_offset_x, m_drag_offset_y;
    bool           m_start_menu_open;
    rbx_module*    m_taskbar;
    rbx_module*    m_startmenu;

    /* Draw a desktop shortcut: 48px icon centred in its slot, label below */
    void draw_icon_slot(int idx) {
        const Shortcut& sc = g_shortcuts[idx];
        int sx = slot_px(idx), sy = slot_py(idx);

        bool hover = m_mouse_px >= sx && m_mouse_px < sx + SLOT_W - 8 &&
                     m_mouse_py >= sy && m_mouse_py < sy + SLOT_H - 6;
        if (hover)
            g_api->fb_fill_rounded_rect(sx, sy, SLOT_W - 8, SLOT_H - 6, 4, 0x30FFFFFFu);

        int ix = sx + (SLOT_W - 8 - ICON_PX) / 2;
        int iy = sy + 8;
        if (sc.icon) {
            g_api->fb_blit_argb_scaled(sc.icon, 32, 32, ix, iy, ICON_PX, ICON_PX);
        } else {
            g_api->fb_fill_rounded_rect(ix, iy, ICON_PX, ICON_PX, 8, fluent::ACCENT);
            char ch[2] = { sc.label[0], '\0' };
            int cw = g_api->fb_text_width(ch, RITOS_FONT_H2);
            g_api->fb_draw_text(ch, ix + (ICON_PX - cw) / 2, iy + 10,
                                RITOS_FONT_H2, 0xFFFFFFFFu);
        }
        /* Label centred below the icon with a soft shadow */
        const char* lbl = sc.label;
        int lw = g_api->fb_text_width(lbl, RITOS_FONT_SMALL);
        int lx = sx + (SLOT_W - 8 - lw) / 2; if (lx < sx - 14) lx = sx - 14;
        int ly = iy + ICON_PX + 6;
        g_api->fb_draw_text(lbl, lx + 1, ly + 1, RITOS_FONT_SMALL, C_LABEL_SH);
        g_api->fb_draw_text(lbl, lx,     ly,     RITOS_FONT_SMALL, C_LABEL);
    }

public:
    Desktop()
        : m_window_count(0), m_mouse_x(0), m_mouse_y(0),
          m_mouse_px(0), m_mouse_py(0), m_mouse_buttons(0),
          m_dragged_window(nullptr), m_drag_offset_x(0), m_drag_offset_y(0),
          m_start_menu_open(false), m_taskbar(nullptr), m_startmenu(nullptr) {
        for (int i = 0; i < 16; i++) m_windows[i] = nullptr;
        if (!g_api->vfs_exists("/users/ramim/configuration/appearance.txt"))
            g_api->vfs_write_file("/users/ramim/configuration/appearance.txt", "W", 1);
        if (g_api->fb_is_available()) {
            m_mouse_px = g_api->fb_get_width()  / 2;
            m_mouse_py = g_api->fb_get_height() / 2;
            m_mouse_x  = m_mouse_px / 8;
            m_mouse_y  = m_mouse_py / 16;
            g_icon_rows = (g_api->fb_get_height() - fluent::TASKBAR_H - GRID_Y0) / SLOT_H;
            if (g_icon_rows < 1) g_icon_rows = 1;
        }
        load_shortcuts();
    }

    int  get_window_count() const { return m_window_count; }
    ritos::Window* get_window(int i) const { return m_windows[i]; }
    int  get_mouse_x() const { return m_mouse_x; }
    int  get_mouse_y() const { return m_mouse_y; }
    int  get_mouse_px_x() const { return m_mouse_px; }
    int  get_mouse_px_y() const { return m_mouse_py; }
    int  is_start_menu_open() const { return m_start_menu_open ? 1 : 0; }
    void set_start_menu_open(int v) { m_start_menu_open = (v != 0); }

    const unsigned int* get_window_icon(int i) const {
        if (i < 0 || i >= m_window_count || !m_windows[i]) return nullptr;
        return static_cast<RbxWindow*>(m_windows[i])->get_icon();
    }

    void add_window(ritos::Window* w) { if (m_window_count < 16) m_windows[m_window_count++] = w; }

    void focus_window(ritos::Window* win) {
        int idx = -1;
        for (int i = 0; i < m_window_count; i++) if (m_windows[i] == win) { idx = i; break; }
        if (idx >= 0) {
            for (int s = idx; s < m_window_count - 1; s++) m_windows[s] = m_windows[s+1];
            m_windows[m_window_count-1] = win;
        }
        for (int i = 0; i < m_window_count; i++)
            if (m_windows[i]) m_windows[i]->set_active(i == m_window_count-1);
    }

    /* Closing a window releases its memory: the module destroys its
     * instance (the app's window object and everything it owns), then we
     * free our wrapper. The next launch reloads the .rbx from scratch. */
    void close_window(ritos::Window* w) {
        int idx = -1;
        for (int i = 0; i < m_window_count; i++) if (m_windows[i] == w) { idx = i; break; }
        if (idx < 0) return;
        for (int i = idx; i < m_window_count - 1; i++) m_windows[i] = m_windows[i+1];
        m_window_count--;
        for (int i = 0; i < m_window_count; i++)
            if (m_windows[i]) m_windows[i]->set_active(i == m_window_count-1);

        RbxWindow* rw = static_cast<RbxWindow*>(w);
        rbx_module* mod = rw->get_module();
        if (mod && mod->destroy && mod->instance) {
            mod->destroy(mod->instance);
            mod->instance = nullptr;
        }
        delete rw;
    }

    void toggle_window(int idx) {
        if (idx < 0 || idx >= m_window_count) return;
        ritos::Window* w = m_windows[idx];
        if (!w->is_visible() || w->is_minimized()) {
            w->set_visible(true); w->set_minimized(false); focus_window(w);
        } else {
            if (w->is_active()) w->set_minimized(true);
            else focus_window(w);
        }
    }

    ritos::Window* get_active_window() {
        for (int i = m_window_count-1; i >= 0; i--)
            if (m_windows[i] && m_windows[i]->is_visible() && !m_windows[i]->is_minimized() && m_windows[i]->is_active())
                return m_windows[i];
        return nullptr;
    }

    void cycle_focus() {
        if (m_window_count <= 0) return;
        int first = -1;
        for (int i = 0; i < m_window_count; i++)
            if (m_windows[i] && m_windows[i]->is_visible() && !m_windows[i]->is_minimized()) { first = i; break; }
        if (first >= 0) {
            ritos::Window* w = m_windows[first];
            for (int i = first; i < m_window_count-1; i++) m_windows[i] = m_windows[i+1];
            m_windows[m_window_count-1] = w;
            for (int i = 0; i < m_window_count; i++)
                if (m_windows[i]) m_windows[i]->set_active(i == m_window_count-1);
        }
    }

    void load_and_run_app(const char* identifier);

    void init_modules(const Desktop_Interface* di) {
        void* e;
        e = g_api->load_rbx("/system/shell/taskbar.rbx");    if (e) m_taskbar   = ((rbx_init_t)e)(g_api, di);
        e = g_api->load_rbx("/system/shell/start_menu.rbx"); if (e) m_startmenu = ((rbx_init_t)e)(g_api, di);
    }

    void forward_key(char key) {
        if (m_start_menu_open && m_startmenu) {
            m_startmenu->handle_key(m_startmenu->instance, key);
            return;
        }
        ritos::Window* aw = get_active_window();
        if (aw) aw->handle_key(key);
    }

    bool start_menu_open() const { return m_start_menu_open; }

    /* px/py are exact pixel coordinates */
    void update_mouse(int px, int py, uint8_t buttons) {
        bool was = (m_mouse_buttons & 1) != 0;
        bool now = (buttons & 1) != 0;
        m_mouse_px = px;     m_mouse_py = py;
        m_mouse_x  = px / 8; m_mouse_y  = py / 16;
        m_mouse_buttons = buttons;
        if (!was && now) handle_mouse_down(px, py);
        else if (was && !now) handle_mouse_up();
        else if (now) handle_mouse_move(px, py);
    }

    void handle_mouse_down(int px_x, int px_y) {
        int sh = g_api->fb_get_height();

        /* Start menu panel (bottom-left, above taskbar) */
        if (m_start_menu_open) {
            if (px_x >= sm_x() && px_x < sm_x() + sm_w() &&
                px_y >= sm_y() && px_y < sm_y() + sm_h()) {
                if (m_startmenu) m_startmenu->handle_click(m_startmenu->instance, px_x, px_y);
                return;
            }
            m_start_menu_open = false;
            /* fall through: the click also acts on whatever is below */
        }

        /* Taskbar (bottom 48px) */
        if (px_y >= sh - fluent::TASKBAR_H) {
            if (m_taskbar) m_taskbar->handle_click(m_taskbar->instance, px_x, px_y);
            return;
        }

        /* Window hit test (pixel-accurate) */
        m_dragged_window = nullptr;
        for (int i = m_window_count-1; i >= 0; i--) {
            ritos::Window* w = m_windows[i];
            if (!w || !w->is_visible() || w->is_minimized()) continue;
            int wpx = w->get_x()*8,     wpy = w->get_y()*16;
            int wpw = w->get_width()*8, wph = w->get_height()*16;
            if (px_x<wpx || px_x>=wpx+wpw || px_y<wpy || px_y>=wpy+wph) continue;

            /* Bring to front */
            if (i < m_window_count-1) {
                for (int k=i; k<m_window_count-1; k++) m_windows[k]=m_windows[k+1];
                m_windows[m_window_count-1] = w;
            }
            for (int k=0; k<m_window_count; k++)
                if (m_windows[k]) m_windows[k]->set_active(k==m_window_count-1);

            if (px_y < wpy + ritos::kTitlebarPx) {
                /* Caption buttons (geometry matches window.cpp):
                 * [minimize][maximize][close], each 46px wide, right edge */
                int x_cl = wpx + wpw - ritos::kCaptionW;
                int x_mx = x_cl - ritos::kCaptionW;
                int x_mn = x_mx - ritos::kCaptionW;
                if (px_x >= x_cl)
                    close_window(w);
                else if (px_x >= x_mx)
                    w->set_maximized(!w->is_maximized());
                else if (px_x >= x_mn)
                    w->set_minimized(true);
                else if (!w->is_maximized()) {
                    m_dragged_window = w;
                    m_drag_offset_x = px_x / 8  - w->get_x();
                    m_drag_offset_y = px_y / 16 - w->get_y();
                }
            } else {
                w->handle_click(px_x, px_y);
            }
            return;
        }

        /* Desktop shortcut icons */
        for (int i = 0; i < g_shortcut_count; i++) {
            int sx = slot_px(i), sy = slot_py(i);
            if (px_x >= sx && px_x < sx + SLOT_W - 8 &&
                px_y >= sy && px_y < sy + SLOT_H - 6) {
                load_and_run_app(g_shortcuts[i].target);
                return;
            }
        }
    }

    void handle_mouse_move(int px_x, int px_y) {
        if (!m_dragged_window || m_dragged_window->is_maximized()) return;
        int nx = px_x / 8  - m_drag_offset_x;
        int ny = px_y / 16 - m_drag_offset_y;
        int grid_w = g_api->fb_get_width()  / 8;
        int max_row = (g_api->fb_get_height() - fluent::TASKBAR_H) / 16 - 1;
        int ww = m_dragged_window->get_width();
        if (nx < -(ww - 4))      nx = -(ww - 4);
        if (nx > grid_w - 4)     nx = grid_w - 4;
        if (ny < 0)              ny = 0;
        if (ny > max_row)        ny = max_row;
        m_dragged_window->set_position(nx, ny);
    }

    void handle_mouse_up() { m_dragged_window = nullptr; }

    bool is_dragging() const { return m_dragged_window != nullptr; }

    /* Zones where pointer hover changes what is on screen — moving the
     * mouse there needs a real redraw; anywhere else only the (cheap)
     * kernel cursor overlay moves. */
    bool in_hot_zone(int px_x, int px_y) const {
        if (!g_api->fb_is_available()) return false;
        if (m_start_menu_open) return true;
        if (px_y >= g_api->fb_get_height() - fluent::TASKBAR_H) return true;
        /* Desktop shortcut hover */
        for (int i = 0; i < g_shortcut_count; i++) {
            int sx = slot_px(i), sy = slot_py(i);
            if (px_x >= sx && px_x < sx + SLOT_W - 8 &&
                px_y >= sy && px_y < sy + SLOT_H - 6) return true;
        }
        /* Caption-button hover on the topmost window under the pointer */
        for (int i = m_window_count - 1; i >= 0; i--) {
            ritos::Window* w = m_windows[i];
            if (!w || !w->is_visible() || w->is_minimized()) continue;
            int wx = w->get_x()*8,     wy = w->get_y()*16;
            int ww = w->get_width()*8, wh = w->get_height()*16;
            if (px_x < wx || px_x >= wx+ww || px_y < wy || px_y >= wy+wh) continue;
            return px_y < wy + ritos::kTitlebarPx;
        }
        return false;
    }

    /* Seconds tick: only the taskbar clock changed — repaint just it */
    void draw_taskbar_tick() {
        if (g_api->fb_is_available() && m_taskbar)
            m_taskbar->draw(m_taskbar->instance);
        else
            draw();
    }

    void draw() {
        if (g_api->fb_is_available()) {
            int sw = g_api->fb_get_width(), sh = g_api->fb_get_height();

            /* Background: wallpaper unless config says solid (or no blob) */
            int clen = 0;
            const char* cfg = g_api->vfs_read_file("/users/ramim/configuration/appearance.txt", &clen);
            bool want_wp = !(cfg && clen > 0 && cfg[0] == 'S');
            if (!(want_wp && g_api->fb_draw_wallpaper()))
                g_api->fb_fill_rect(0, 0, sw, sh, C_DESK_SOLID);

            for (int i = 0; i < g_shortcut_count; i++) draw_icon_slot(i);

            for (int i = 0; i < m_window_count; i++)
                if (m_windows[i]) m_windows[i]->draw();
            if (m_taskbar)  m_taskbar->draw(m_taskbar->instance);
            if (m_start_menu_open && m_startmenu) m_startmenu->draw(m_startmenu->instance);
        } else {
            /* VGA fallback: solid background */
            int grid_w = 80, grid_h = 25;
            for (int y = 0; y < grid_h - 1; y++)
                for (int x = 0; x < grid_w; x++)
                    g_api->draw_char(' ', (uint8_t)rit::Color::Black, (uint8_t)rit::Color::Black, x, y);

            for (int i = 0; i < m_window_count; i++)
                if (m_windows[i]) m_windows[i]->draw();
            if (m_taskbar)  m_taskbar->draw(m_taskbar->instance);
            if (m_start_menu_open && m_startmenu) m_startmenu->draw(m_startmenu->instance);
        }
    }

    /* Windows-style arrow cursor: a kernel overlay stamped into VRAM at
     * flush time, so moving it never requires repainting the scene. */
    void draw_cursor() {
        if (g_api->fb_is_available()) {
            if (!g_cursor_built) {
                build_cursor();
                g_api->fb_set_cursor_image(g_cursor_px, CUR_W, CUR_H);
            }
            g_api->fb_set_cursor_pos(m_mouse_px, m_mouse_py);
        } else {
            /* VGA fallback cursor */
            uint16_t* vm = g_api->get_screen_buffer();
            if (!vm) vm = (uint16_t*)0xB8000;
            int idx = m_mouse_y * 80 + m_mouse_x;
            if (idx >= 0 && idx < 80 * 25)
                vm[idx] = 0x1E | ((uint16_t)0x0F << 8);
        }
    }
};

static Desktop* g_desktop = nullptr;

/* Desktop Interface API */
static int  di_get_window_count()        { return g_desktop->get_window_count(); }
static const char* di_get_window_title(int i) { return g_desktop->get_window(i)->get_title(); }
static int  di_is_window_active(int i)   { return g_desktop->get_window(i)->is_active()    ? 1:0; }
static int  di_is_window_visible(int i)  { return g_desktop->get_window(i)->is_visible()   ? 1:0; }
static int  di_is_window_minimized(int i){ return g_desktop->get_window(i)->is_minimized() ? 1:0; }
static void di_toggle_window(int i)      { g_desktop->toggle_window(i); }
static void di_launch_app(const char* t) { g_desktop->load_and_run_app(t); }
static int  di_get_mouse_x()             { return g_desktop->get_mouse_x(); }
static int  di_get_mouse_y()             { return g_desktop->get_mouse_y(); }
static int  di_get_mouse_px_x()          { return g_desktop->get_mouse_px_x(); }
static int  di_get_mouse_px_y()          { return g_desktop->get_mouse_px_y(); }
static int  di_is_start_menu_open()      { return g_desktop->is_start_menu_open(); }
static void di_set_start_menu_open(int v){ g_desktop->set_start_menu_open(v); }
static const unsigned int* di_get_window_icon(int i) { return g_desktop->get_window_icon(i); }

static Desktop_Interface g_di = {
    .get_window_count  = di_get_window_count,
    .get_window_title  = di_get_window_title,
    .is_window_active  = di_is_window_active,
    .is_window_visible = di_is_window_visible,
    .is_window_minimized = di_is_window_minimized,
    .toggle_window     = di_toggle_window,
    .launch_app        = di_launch_app,
    .get_mouse_x       = di_get_mouse_x,
    .get_mouse_y       = di_get_mouse_y,
    .is_start_menu_open= di_is_start_menu_open,
    .set_start_menu_open = di_set_start_menu_open,
    .get_mouse_px_x    = di_get_mouse_px_x,
    .get_mouse_px_y    = di_get_mouse_px_y,
    .get_window_icon   = di_get_window_icon,
};

void Desktop::load_and_run_app(const char* id) {
    const char* path = id;
    if (str_equals(id,"System Monitor")||str_equals(id,"Monitor")) path="/system/executables/system_monitor.rbx";
    else if (str_equals(id,"Calculator")||str_equals(id,"Calc"))   path="/system/executables/calculator.rbx";
    else if (str_equals(id,"Text Editor")||str_equals(id,"Editor"))path="/system/executables/text_editor.rbx";
    else if (str_equals(id,"File Explorer")||str_equals(id,"Files"))path="/system/executables/file_manager.rbx";
    else if (str_equals(id,"Clock"))                                path="/system/executables/clock.rbx";
    else if (str_equals(id,"Calendar")||str_equals(id,"Calen"))    path="/system/executables/calendar.rbx";
    else if (str_equals(id,"Settings")||str_equals(id,"Config"))   path="/system/executables/settings.rbx";
    else if (str_equals(id,"Terminal")||str_equals(id,"Term"))     path="/system/executables/terminal.rbx";
    else if (str_equals(id,"Image Viewer")||str_equals(id,"ImgView")) path="/system/executables/image_viewer.rbx";

    for (int i = 0; i < m_window_count; i++) {
        RbxWindow* rw = static_cast<RbxWindow*>(m_windows[i]);
        if (rw && str_equals(rw->get_rbx_path(), path)) {
            rw->set_visible(true); rw->set_minimized(false); focus_window(rw);
            draw(); draw_cursor(); return;
        }
    }

    void* entry = g_api->load_rbx(path);
    if (entry) {
        rbx_module* mod = ((rbx_init_t)entry)(g_api, &g_di);
        if (mod && mod->type == RBX_MODULE_APP_WINDOW) {
            RbxWindow* rw = new RbxWindow(mod, path);
            add_window(rw); focus_window(rw);
            draw(); draw_cursor();
        }
    }
}

static void desktop_launch_handler(const char* title) {
    if (g_desktop) g_desktop->load_and_run_app(title);
}

extern "C" void _start(const RitOS_API* api) {
    g_api = api;
    api->enable_double_buffer(1);
    rit::System::set_launch_app_handler(desktop_launch_handler);
    ritos::Window::set_desktop_interface(&g_di);

    g_desktop = new Desktop();
    g_desktop->init_modules(&g_di);

    g_desktop->draw();
    g_desktop->draw_cursor();
    api->flush();

    int prev_mx = g_desktop->get_mouse_px_x(), prev_my = g_desktop->get_mouse_px_y();
    uint8_t prev_buttons=0;
    bool was_hot=false;
    int prev_sec=0;
    { int h=0,m=0,s=0; api->get_time(&h,&m,&s); prev_sec=s; }

    while (1) {
        int mx=prev_mx, my=prev_my;
        uint8_t buttons=prev_buttons;

        if (api->poll_mouse_px(&mx, &my, &buttons)) {
            if (mx!=prev_mx || my!=prev_my || buttons!=prev_buttons) {
                bool clicked = buttons != prev_buttons;
                bool hot     = g_desktop->in_hot_zone(mx, my);
                g_desktop->update_mouse(mx, my, buttons);
                /* Full scene repaint only when something can have changed:
                 * a click, an active drag, or pointer hover feedback.
                 * Plain moves just slide the kernel cursor overlay. */
                if (clicked || g_desktop->is_dragging() || hot || was_hot)
                    g_desktop->draw();
                g_desktop->draw_cursor();
                api->flush();
                was_hot = hot;
                prev_mx=mx; prev_my=my; prev_buttons=buttons;
            }
        }

        char key=0;
        if (api->poll_keyboard(&key)) {
            if (key=='\t' && !g_desktop->start_menu_open()) {
                g_desktop->cycle_focus();
            } else if (key==27) {
                if (g_desktop->start_menu_open()) {
                    g_desktop->set_start_menu_open(0);
                } else {
                    ritos::Window* aw = g_desktop->get_active_window();
                    if (aw) g_desktop->close_window(aw);
                }
            } else {
                g_desktop->forward_key(key);
            }
            g_desktop->draw();
            g_desktop->draw_cursor();
            api->flush();
        }

        int ch=0,cm=0,cs=0;
        api->get_time(&ch,&cm,&cs);
        if (cs != prev_sec) {
            g_desktop->draw_taskbar_tick();
            g_desktop->draw_cursor();
            api->flush();
            prev_sec = cs;
        }

        for (volatile uint32_t i = 0; i < 40000; i++) __asm__ volatile("nop");
    }
}

#include "../../gui/include/ritos/window.hpp"
#include "../../framework/include/rit/system.hpp"
#include "../../framework/include/rit/rbx_module.h"
#include "../../../assets/icons/icons_32.h"
#include <stdint.h>

extern const RitOS_API* g_api;

/* ── color / layout constants ─────────────────────────────────────────── */
#define C_DESK_TOP   0xFF0D1B3Au  /* deep midnight blue */
#define C_DESK_BOT   0xFF142440u  /* rich navy */
#define C_TOPBAR1    0xF00A1526u  /* near-black teal */
#define C_TOPBAR2    0xF0111E36u
#define C_ICON_BG    0x44B0C8FFu  /* subtle blue glow on hover */
#define C_LABEL      0xFFD8E8FFu
#define C_SHADOW     0x70000000u

/* Grid: 128 cols × 48 rows (1024/8 × 768/16) */
#define GRID_W  128
#define GRID_H  48
#define TOPBAR_ROW  0   /* status bar on row 0 */
#define TASKBAR_ROW 45  /* taskbar rows 45-47 */

/* Icon layout: 2 columns × 4 rows of 32×32 icons */
/* Pixel positions: col1 at px=20, col2 at px=80; rows starting at py=40, spacing=72 */
struct IconSlot { int px; int py; int app_idx; };

static const IconSlot g_icon_slots[8] = {
    {20,  40, 0},  /* col1 row1: sysmon   */
    {20, 120, 1},  /* col1 row2: calc     */
    {20, 200, 2},  /* col1 row3: editor   */
    {20, 280, 3},  /* col1 row4: files    */
    {80,  40, 4},  /* col2 row1: clock    */
    {80, 120, 5},  /* col2 row2: calendar */
    {80, 200, 6},  /* col2 row3: settings */
    {80, 280, 7},  /* col2 row4: terminal */
};

static const char* const g_app_paths[9] = {
    "/sys/sysmon.rbx", "/sys/calculator.rbx", "/sys/texteditor.rbx",
    "/sys/filemanager.rbx", "/sys/clock.rbx", "/sys/calendar.rbx",
    "/sys/settings.rbx", "/sys/terminal.rbx", "/sys/imgview.rbx"
};

/* Map icon_idx to the icon pixel array in icons_32.h */
static const uint32_t* const g_icon_px[9] = {
    ICON_SYSMON, ICON_CALC, ICON_EDITOR,
    ICON_FILES, ICON_CLOCK, ICON_CALENDAR,
    ICON_SETTINGS, ICON_TERMINAL, ICON_IMGVIEW
};

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

/* ── RbxWindow ────────────────────────────────────────────────────────── */
class RbxWindow : public ritos::Window {
    rbx_module* m_mod;
    char        m_rbx_path[64];
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
    }
    const char* get_rbx_path() const { return m_rbx_path; }
    rbx_module* get_module()   const { return m_mod; }

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
    rbx_module*    m_statusbar;

    /* Draw a 32×32 icon and label below it */
    void draw_icon_slot(int idx) {
        const IconSlot& sl = g_icon_slots[idx];
        g_api->fb_blit_argb(g_icon_px[sl.app_idx], sl.px, sl.py, 32, 32);
        /* Label centered below icon */
        const char* lbl = g_icon_labels[sl.app_idx];
        int llen = 0; while (lbl[llen]) llen++;
        int lx = sl.px + (32 - llen * 8) / 2;
        g_api->fb_draw_string_px(lbl, C_LABEL, 0, lx, sl.py + 34, 1);
    }

public:
    Desktop()
        : m_window_count(0), m_mouse_x(64), m_mouse_y(24),
          m_mouse_px(512), m_mouse_py(384), m_mouse_buttons(0),
          m_dragged_window(nullptr), m_drag_offset_x(0), m_drag_offset_y(0),
          m_start_menu_open(false), m_taskbar(nullptr), m_startmenu(nullptr), m_statusbar(nullptr) {
        for (int i = 0; i < 16; i++) m_windows[i] = nullptr;
        if (!g_api->vfs_exists("/sys/settings.cfg"))
            g_api->vfs_write_file("/sys/settings.cfg", "PD", 2);
    }

    int  get_window_count() const { return m_window_count; }
    ritos::Window* get_window(int i) const { return m_windows[i]; }
    int  get_mouse_x() const { return m_mouse_x; }
    int  get_mouse_y() const { return m_mouse_y; }
    int  get_mouse_px_x() const { return m_mouse_px; }
    int  get_mouse_px_y() const { return m_mouse_py; }
    int  is_start_menu_open() const { return m_start_menu_open ? 1 : 0; }
    void set_start_menu_open(int v) { m_start_menu_open = (v != 0); }

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
        e = g_api->load_rbx("/sys/taskbar.rbx");   if (e) m_taskbar   = ((rbx_init_t)e)(g_api, di);
        e = g_api->load_rbx("/sys/startmenu.rbx"); if (e) m_startmenu = ((rbx_init_t)e)(g_api, di);
        e = g_api->load_rbx("/sys/statusbars.rbx");if (e) m_statusbar = ((rbx_init_t)e)(g_api, di);
    }

    /* px/py are exact pixel coordinates */
    void update_mouse(int px, int py, uint8_t buttons) {
        bool was = (m_mouse_buttons & 1) != 0;
        bool now = (buttons & 1) != 0;
        m_mouse_px = px;     m_mouse_py = py;
        m_mouse_x  = px / 8; m_mouse_y  = py / 16;
        m_mouse_buttons = buttons;
        if (!was && now) handle_mouse_down(px, py);
        else if (was && !now) handle_mouse_up();
        else if (now) handle_mouse_move(m_mouse_x, m_mouse_y);
    }

    void handle_mouse_down(int px_x, int px_y) {
        int x = px_x / 8;   /* char-cell coords for legacy hit tests */
        int y = px_y / 16;
        /* Start menu area */
        if (m_start_menu_open) {
            if (x >= 1 && x <= 20 && y >= 27 && y <= 44) {
                if (m_startmenu) m_startmenu->handle_click(m_startmenu->instance, x, y);
                draw(); return;
            }
            m_start_menu_open = false; draw();
        }

        /* Taskbar */
        if (y >= TASKBAR_ROW) {
            if (m_taskbar) m_taskbar->handle_click(m_taskbar->instance, x, y);
            draw(); return;
        }

        /* Window hit test (pixel-accurate) */
        m_dragged_window = nullptr;
        for (int i = m_window_count-1; i >= 0; i--) {
            ritos::Window* w = m_windows[i];
            if (!w || !w->is_visible() || w->is_minimized()) continue;
            int wx=w->get_x(), wy=w->get_y(), ww=w->get_width(), wh=w->get_height();
            int wpx=wx*8, wpy=wy*16, wpw=ww*8, wph=wh*16;
            if (px_x<wpx || px_x>=wpx+wpw || px_y<wpy || px_y>=wpy+wph) continue;

            /* Bring to front */
            if (i < m_window_count-1) {
                for (int k=i; k<m_window_count-1; k++) m_windows[k]=m_windows[k+1];
                m_windows[m_window_count-1] = w;
            }
            for (int k=0; k<m_window_count; k++)
                if (m_windows[k]) m_windows[k]->set_active(k==m_window_count-1);

            if (px_y < wpy + 24) { /* 24px title bar */
                /* Button centres must match the chrome in window.cpp:
                 * close at pw-16, minimize at pw-36, maximize at pw-56 */
                int dy = px_y - (wpy + 12);
                int d_cl = px_x - (wpx + wpw - 16);
                int d_mn = px_x - (wpx + wpw - 36);
                int d_mx = px_x - (wpx + wpw - 56);
                if (dy >= -9 && dy <= 9 && d_cl >= -9 && d_cl <= 9)
                    w->set_visible(false);
                else if (dy >= -9 && dy <= 9 && d_mn >= -9 && d_mn <= 9)
                    w->set_minimized(true);
                else if (dy >= -9 && dy <= 9 && d_mx >= -9 && d_mx <= 9)
                    w->set_maximized(!w->is_maximized());
                else if (!w->is_maximized()) {
                    m_dragged_window = w;
                    m_drag_offset_x = x - wx;
                    m_drag_offset_y = y - wy;
                }
            } else {
                w->handle_click(x, y);
            }
            draw(); return;
        }

        /* Icon click (col1: x=1..9, col2: x=10..19) */
        /* Each icon row in char-grid: row0=y∈[2,7], row1=[7,12], row2=[12,17], row3=[17,22] */
        int col = -1, row = -1;
        if (x >= 1 && x <= 9)   col = 0;
        else if (x >= 10 && x <= 18) col = 1;

        if (col >= 0) {
            if      (y >= 2  && y <= 7)  row = 0;
            else if (y >= 8  && y <= 12) row = 1;
            else if (y >= 13 && y <= 17) row = 2;
            else if (y >= 18 && y <= 22) row = 3;

            if (row >= 0) {
                int app_idx = col * 4 + row;
                load_and_run_app(g_app_paths[app_idx]);
            }
        }
    }

    void handle_mouse_move(int x, int y) {
        if (!m_dragged_window || m_dragged_window->is_maximized()) return;
        int nx = x - m_drag_offset_x;
        int ny = y - m_drag_offset_y;
        if (nx < -10) nx = -10;
        if (nx > 110) nx = 110;
        if (ny < 2)   ny = 2;
        if (ny > 44)  ny = 44;
        m_dragged_window->set_position(nx, ny);
        draw();
    }

    void handle_mouse_up() { m_dragged_window = nullptr; }

    void draw() {
        if (g_api->fb_is_available()) {
            /* Full-screen gradient background: covers entire 1024x768 */
            g_api->fb_fill_grad_v(0, 0, 1024, 768, C_DESK_TOP, C_DESK_BOT);

            /* Draw desktop icons */
            for (int i = 0; i < 8; i++) draw_icon_slot(i);

            if (m_statusbar) m_statusbar->draw(m_statusbar->instance);
            for (int i = 0; i < m_window_count; i++)
                if (m_windows[i]) m_windows[i]->draw();
            if (m_taskbar)  m_taskbar->draw(m_taskbar->instance);
            if (m_start_menu_open && m_startmenu) m_startmenu->draw(m_startmenu->instance);
        } else {
            /* VGA fallback: solid background */
            for (int y = 1; y < TASKBAR_ROW; y++)
                for (int x = 0; x < GRID_W; x++)
                    g_api->draw_char(' ', (uint8_t)rit::Color::Black, (uint8_t)rit::Color::Black, x, y);

            if (m_statusbar) m_statusbar->draw(m_statusbar->instance);
            for (int i = 0; i < m_window_count; i++)
                if (m_windows[i]) m_windows[i]->draw();
            if (m_taskbar)  m_taskbar->draw(m_taskbar->instance);
            if (m_start_menu_open && m_startmenu) m_startmenu->draw(m_startmenu->instance);
        }
    }

    /* Windows-style arrow cursor at exact pixel position */
    void draw_cursor() {
        if (g_api->fb_is_available()) {
            if (!g_cursor_built) build_cursor();
            g_api->fb_blit_argb(g_cursor_px, m_mouse_px, m_mouse_py, CUR_W, CUR_H);
        } else {
            /* VGA fallback cursor */
            uint16_t* vm = g_api->get_screen_buffer();
            if (!vm) vm = (uint16_t*)0xB8000;
            int idx = m_mouse_y * GRID_W + m_mouse_x;
            if (idx >= 0 && idx < GRID_W * GRID_H)
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
};

void Desktop::load_and_run_app(const char* id) {
    const char* path = id;
    if (str_equals(id,"System Monitor")||str_equals(id,"Monitor")) path="/sys/sysmon.rbx";
    else if (str_equals(id,"Calculator")||str_equals(id,"Calc"))   path="/sys/calculator.rbx";
    else if (str_equals(id,"Text Editor")||str_equals(id,"Editor"))path="/sys/texteditor.rbx";
    else if (str_equals(id,"File Explorer")||str_equals(id,"Files"))path="/sys/filemanager.rbx";
    else if (str_equals(id,"Clock"))                                path="/sys/clock.rbx";
    else if (str_equals(id,"Calendar")||str_equals(id,"Calen"))    path="/sys/calendar.rbx";
    else if (str_equals(id,"Settings")||str_equals(id,"Config"))   path="/sys/settings.rbx";
    else if (str_equals(id,"Terminal")||str_equals(id,"Term"))     path="/sys/terminal.rbx";
    else if (str_equals(id,"Image Viewer")||str_equals(id,"ImgView")) path="/sys/imgview.rbx";

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

    g_desktop = new Desktop();
    g_desktop->init_modules(&g_di);

    g_desktop->draw();
    g_desktop->draw_cursor();
    api->flush();

    int prev_mx=512, prev_my=384;
    uint8_t prev_buttons=0;
    int prev_sec=0;
    { int h=0,m=0,s=0; api->get_time(&h,&m,&s); prev_sec=s; }

    while (1) {
        int mx=prev_mx, my=prev_my;
        uint8_t buttons=prev_buttons;

        if (api->poll_mouse_px(&mx, &my, &buttons)) {
            if (mx!=prev_mx || my!=prev_my || buttons!=prev_buttons) {
                g_desktop->update_mouse(mx, my, buttons);
                g_desktop->draw();
                g_desktop->draw_cursor();
                api->flush();
                prev_mx=mx; prev_my=my; prev_buttons=buttons;
            }
        }

        char key=0;
        if (api->poll_keyboard(&key)) {
            if (key=='\t') {
                g_desktop->cycle_focus();
            } else if (key==27) {
                ritos::Window* aw = g_desktop->get_active_window();
                if (aw) aw->set_visible(false);
            } else {
                ritos::Window* aw = g_desktop->get_active_window();
                if (aw) aw->handle_key(key);
            }
            g_desktop->draw();
            g_desktop->draw_cursor();
            api->flush();
        }

        int ch=0,cm=0,cs=0;
        api->get_time(&ch,&cm,&cs);
        if (cs != prev_sec) {
            g_desktop->draw();
            g_desktop->draw_cursor();
            api->flush();
            prev_sec = cs;
        }

        for (volatile uint32_t i = 0; i < 40000; i++) __asm__ volatile("nop");
    }
}

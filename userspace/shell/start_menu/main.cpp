#include <ritos/fluent.hpp>
#include <rit/system.hpp>
#include <rit/rbx_module.h>
#include <rit/rbx_format.h>

extern const RitOS_API* g_api;

/*
 * Windows-11-style start menu, opening at the BOTTOM-LEFT (per user
 * preference) above the taskbar: search box on top, "Pinned" grid of
 * apps (4 columns, 32px icons), user row + Restart/Power at the bottom.
 *
 * Panel geometry mirrors the desktop's hit-test (sm_x/sm_y/sm_w/sm_h
 * in shell/desktop/main.cpp) — keep the two in sync.
 */

#define SM_X     8
#define GRID_COLS 4
#define CELL_H   96
#define FOOT_H   56

static int sm_w() { return 560; }
static int sm_h() {
    int h = 620;
    int avail = g_api->fb_get_height() - fluent::TASKBAR_H - 16;
    return h > avail ? avail : h;
}
static int sm_y() { return g_api->fb_get_height() - fluent::TASKBAR_H - 8 - sm_h(); }

/* Menu entries are /launcher/*.stct shortcut files: the label is the
 * shortcut filename (extension hidden), the icon comes embedded in the
 * target .rbx. */
#define SM_MAX_ITEMS 12

struct SmItem {
    char            label[24];
    char            target[64];
    const uint32_t* icon;   /* 32x32 ARGB inside the .rbx in VFS */
};
static SmItem sm_items[SM_MAX_ITEMS];
static int    sm_count = 0;

static char g_search[28];
static int  g_search_len = 0;

static void sm_load_items() {
    sm_count = 0;
    const char* files[64];
    int n = g_api->vfs_get_file_list(files, 64);
    for (int i = 0; i < n && sm_count < SM_MAX_ITEMS; i++) {
        const char* f = files[i];
        const char* pre = "/users/ramim/launcher/";
        int p = 0; while (pre[p] && f[p] == pre[p]) p++;
        if (pre[p]) continue;
        int fl = 0; while (f[fl]) fl++;
        if (fl < 6 || f[fl-5]!='.' || f[fl-4]!='s' || f[fl-3]!='t' || f[fl-2]!='c' || f[fl-1]!='t')
            continue;

        SmItem& it = sm_items[sm_count];
        int li = 0;
        for (int k = p; k < fl - 5 && li < 23; k++) it.label[li++] = (f[k] == '_') ? ' ' : f[k];
        it.label[li] = '\0';

        int clen = 0;
        const char* c = g_api->vfs_read_file(f, &clen);
        if (!c || clen <= 0) continue;
        int ti = 0;
        while (ti < clen && ti < 63 && c[ti] && c[ti] != '\n' && c[ti] != '\r') {
            it.target[ti] = c[ti]; ti++;
        }
        it.target[ti] = '\0';

        it.icon = nullptr;
        int len = 0;
        const char* data = g_api->vfs_read_file(it.target, &len);
        if (data && len >= (int)sizeof(rbx_header_t)) {
            const rbx_header_t* h = (const rbx_header_t*)data;
            if (h->magic[0]=='R' && h->magic[1]=='B' && h->magic[2]=='X' && h->magic[3]=='2' &&
                h->icon_size == RBX_ICON_BYTES &&
                h->icon_offset + h->icon_size <= (uint32_t)len)
                it.icon = (const uint32_t*)(data + h->icon_offset);
        }
        sm_count++;
    }
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static bool item_matches(const SmItem& it) {
    if (g_search_len == 0) return true;
    int ll = 0; while (it.label[ll]) ll++;
    for (int s = 0; s + g_search_len <= ll; s++) {
        int k = 0;
        while (k < g_search_len && lower(it.label[s+k]) == lower(g_search[k])) k++;
        if (k == g_search_len) return true;
    }
    return false;
}

/* Filtered index list; returns count */
static int filtered(int* out) {
    int n = 0;
    for (int i = 0; i < sm_count; i++)
        if (item_matches(sm_items[i])) out[n++] = i;
    return n;
}

/* Grid cell rect for the k-th visible item */
static void cell_rect(int k, int* x, int* y, int* w) {
    int cw = (sm_w() - 48) / GRID_COLS;
    *w = cw;
    *x = SM_X + 24 + (k % GRID_COLS) * cw;
    *y = sm_y() + 112 + (k / GRID_COLS) * CELL_H;
}

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
    g_api = api;
    sm_load_items();
    g_search_len = 0; g_search[0] = '\0';
    static rbx_module mod;
    mod.type = RBX_MODULE_STARTMENU;
    const char* nm = "Start Menu"; int i=0;
    while(nm[i]&&i<31){mod.name[i]=nm[i];i++;} mod.name[i]='\0';
    mod.instance = (void*)desktop;

    mod.draw = [](void* inst) {
        const Desktop_Interface* d = static_cast<const Desktop_Interface*>(inst);

        if (!g_api->fb_is_available()) {
            /* VGA fallback: simple list */
            uint8_t dbg = (uint8_t)rit::Color::DarkGrey;
            for (int row=0; row<sm_count+3; row++)
                for (int col=0; col<22; col++)
                    g_api->draw_char(' ', 15, dbg, 1+col, 5+row);
            for (int i=0; i<sm_count; i++)
                for (int c=0; sm_items[i].label[c]; c++)
                    g_api->draw_char(sm_items[i].label[c], 15, dbg, 2+c, 6+i);
            return;
        }

        int x = SM_X, y = sm_y(), w = sm_w(), h = sm_h();
        int mx = d->get_mouse_px_x(), my = d->get_mouse_px_y();

        /* Shadow + acrylic panel + border */
        g_api->fb_fill_rect_blend(x+4, y+6, w, h, 0x30000000u);
        fluent::rrect(x-1, y-1, w+2, h+2, 9, fluent::BORDER_DIM);
        fluent::rrect(x, y, w, h, 8, fluent::MENU_BG);

        /* Search box */
        int sx = x + 24, sy = y + 20, sw2 = w - 48, sh2 = 36;
        fluent::rrect_border(sx, sy, sw2, sh2, 6, fluent::BORDER_DIM, fluent::CARD);
        g_api->fb_draw_hline_px(sx + 6, sy + sh2 - 2, sw2 - 12, fluent::ACCENT);
        fluent::glyph_search(sx + 12, sy + 11, 14, fluent::TEXT_SEC, fluent::CARD);
        if (g_search_len > 0) {
            fluent::text(g_search, fluent::TEXT, sx + 36, sy + 10);
            /* caret */
            fluent::rect(sx + 36 + g_search_len * 8 + 1, sy + 9, 1, 18, fluent::TEXT);
        } else {
            fluent::text("Type here to search", fluent::TEXT_DIS, sx + 36, sy + 10);
        }

        /* Pinned label */
        fluent::text("Pinned", fluent::TEXT, x + 28, y + 80);

        /* App grid (filtered by search) */
        int idx[SM_MAX_ITEMS];
        int n = filtered(idx);
        int foot_y = y + h - FOOT_H;
        for (int k = 0; k < n; k++) {
            int cx, cy, cw;
            cell_rect(k, &cx, &cy, &cw);
            if (cy + CELL_H > foot_y - 4) break;

            const SmItem& it = sm_items[idx[k]];
            bool hov = (mx >= cx && mx < cx + cw && my >= cy && my < cy + CELL_H);
            if (hov) fluent::rrect(cx, cy, cw, CELL_H - 6, 5, fluent::HOVER);

            int ix = cx + (cw - 32) / 2;
            if (it.icon)
                g_api->fb_blit_argb(it.icon, ix, cy + 12, 32, 32);
            else
                fluent::glyph_app(ix, cy + 12, 32);

            int ll = 0; while (it.label[ll]) ll++;
            int lx = cx + (cw - ll * 8) / 2; if (lx < cx + 2) lx = cx + 2;
            fluent::text(it.label, fluent::TEXT, lx, cy + 52);
        }
        if (n == 0)
            fluent::text("No results", fluent::TEXT_DIS, x + 28, y + 116);

        /* Footer: user row + restart/power */
        fluent::rect(x + 1, foot_y, w - 2, 1, fluent::STROKE);
        fluent::rect(x + 1, foot_y + 1, w - 2, FOOT_H - 9, 0xFFEFEFEFu);
        fluent::rect(x + 1, foot_y + FOOT_H - 9, w - 2, 1, 0xFFEFEFEFu);

        /* avatar circle + username */
        int ay = foot_y + FOOT_H / 2 - 1;
        rit::System::fb_fill_circle(x + 40, ay, 14, fluent::ACCENT);
        fluent::text("r", fluent::TEXT_WHITE, x + 37, ay - 8);
        fluent::text("ramim", fluent::TEXT, x + 64, ay - 8);

        /* restart + power buttons (36x36) */
        int pwx = x + w - 24 - 36;
        int rsx = pwx - 36 - 8;
        bool hov_pw = (mx >= pwx && mx < pwx + 36 && my >= foot_y + 8 && my < foot_y + 44);
        bool hov_rs = (mx >= rsx && mx < rsx + 36 && my >= foot_y + 8 && my < foot_y + 44);
        if (hov_rs) fluent::rrect(rsx, foot_y + 8, 36, 36, 5, fluent::HOVER);
        if (hov_pw) fluent::rrect(pwx, foot_y + 8, 36, 36, 5, fluent::HOVER);
        uint32_t rs_bg = hov_rs ? fluent::HOVER : 0xFFEFEFEFu;
        uint32_t pw_bg = hov_pw ? fluent::HOVER : 0xFFEFEFEFu;
        fluent::glyph_restart(rsx + 10, foot_y + 18, 16, fluent::TEXT, rs_bg);
        fluent::glyph_power(pwx + 10, foot_y + 18, 16, fluent::TEXT, pw_bg);
    };

    /* mx/my are exact pixel coordinates */
    mod.handle_click = [](void* inst, int mx, int my) {
        Desktop_Interface* d = static_cast<Desktop_Interface*>(inst);

        if (!g_api->fb_is_available()) {
            int idx = my - 6;
            if (idx >= 0 && idx < sm_count) {
                d->set_start_menu_open(0);
                d->launch_app(sm_items[idx].target);
            }
            return;
        }

        int x = SM_X, y = sm_y(), w = sm_w(), h = sm_h();
        int foot_y = y + h - FOOT_H;

        /* Search box: keep the menu open, typing goes through handle_key */
        if (my >= y + 20 && my < y + 56) return;

        /* Footer buttons */
        if (my >= foot_y) {
            int pwx = x + w - 24 - 36;
            int rsx = pwx - 36 - 8;
            if (mx >= pwx && mx < pwx + 36 && my >= foot_y + 8 && my < foot_y + 44)
                g_api->shutdown();
            else if (mx >= rsx && mx < rsx + 36 && my >= foot_y + 8 && my < foot_y + 44)
                g_api->reboot();
            return;
        }

        /* App grid */
        int idx[SM_MAX_ITEMS];
        int n = filtered(idx);
        for (int k = 0; k < n; k++) {
            int cx, cy, cw;
            cell_rect(k, &cx, &cy, &cw);
            if (cy + CELL_H > foot_y - 4) break;
            if (mx >= cx && mx < cx + cw && my >= cy && my < cy + CELL_H) {
                g_search_len = 0; g_search[0] = '\0';
                d->set_start_menu_open(0);
                d->launch_app(sm_items[idx[k]].target);
                return;
            }
        }
    };

    /* Typed input feeds the search box while the menu is open */
    mod.handle_key = [](void* inst, char key) {
        Desktop_Interface* d = static_cast<Desktop_Interface*>(inst);
        if (key == '\b') {
            if (g_search_len > 0) g_search[--g_search_len] = '\0';
        } else if (key == '\n') {
            int idx[SM_MAX_ITEMS];
            int n = filtered(idx);
            if (n > 0) {
                int target = idx[0];
                g_search_len = 0; g_search[0] = '\0';
                d->set_start_menu_open(0);
                d->launch_app(sm_items[target].target);
            }
        } else if (key >= 32 && key < 127) {
            if (g_search_len < (int)sizeof(g_search) - 1) {
                g_search[g_search_len++] = key;
                g_search[g_search_len] = '\0';
            }
        }
    };

    return &mod;
}

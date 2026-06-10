#include "../../gui/include/ritos/window.hpp"
#include "../../framework/include/rit/system.hpp"
#include "../../framework/include/rit/rbx_module.h"

extern const RitOS_API* g_api;

/* color / layout */
#define C_TB1        0xF0101E38u  /* dark midnight blue */
#define C_TB2        0xF0182A4Eu  /* slightly richer */
#define C_START1     0xFF2C4B8Au  /* nice blue start btn */
#define C_START2     0xFF1E3464u
#define C_START_HOV  0xFF4070C0u
#define C_BTN_ACT    0xFF1E3060u  /* active window btn */
#define C_BTN_NORM   0xFF14233Eu
#define C_BTN_HOV    0xFF243A6Au
#define C_ACCENT     0xFF5B8FFFu
#define C_TEXT       0xFFECF0FFu
#define C_TEXT_DIM   0xFF7A96C4u
#define C_CLOCK      0xFF38D4C8u
#define TASKBAR_PY   728   /* pixel y of taskbar (768-40) */
#define TASKBAR_H    40
#define TASKBAR_ROW  45    /* first char row of taskbar */

static bool str_starts(const char* str, const char* pre) {
    int i=0; while(pre[i]){if(str[i]!=pre[i])return false;i++;} return true;
}
static const char* short_label(const char* t) {
    if(str_starts(t,"System Monitor")) return "SysMon";
    if(str_starts(t,"Calculator"))     return "Calc";
    if(str_starts(t,"Text Editor"))    return "Editor";
    if(str_starts(t,"File Explorer"))  return "Files";
    if(str_starts(t,"Clock"))          return "Clock";
    if(str_starts(t,"Calendar"))       return "Calen";
    if(str_starts(t,"Settings"))       return "Config";
    if(str_starts(t,"Terminal"))       return "Term";
    return t;
}

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
    g_api = api;
    static rbx_module mod;
    mod.type = RBX_MODULE_TASKBAR;
    const char* nm = "Task Bar"; int i=0;
    while(nm[i]&&i<31){mod.name[i]=nm[i];i++;} mod.name[i]='\0';
    mod.instance = (void*)desktop;

    mod.draw = [](void* inst) {
        const Desktop_Interface* d = static_cast<const Desktop_Interface*>(inst);

        if (g_api->fb_is_available()) {
            /* ── Pixel taskbar ── */
            g_api->fb_fill_grad_v(0, TASKBAR_PY, 1024, TASKBAR_H, C_TB1, C_TB2);

            int mx_px = d->get_mouse_x() * 8;
            int my    = d->get_mouse_y();
            int sm_open = d->is_start_menu_open();

            /* Start button (pixel x=4..84, y=TASKBAR_PY+4..TASKBAR_PY+36) */
            bool start_hov = (my >= TASKBAR_ROW && mx_px >= 4 && mx_px < 84);
            uint32_t sb1 = (sm_open || start_hov) ? C_START_HOV : C_START1;
            uint32_t sb2 = (sm_open || start_hov) ? C_START2     : C_START2;
            g_api->fb_fill_grad_v(4, TASKBAR_PY+4, 80, 32, sb1, sb2);
            g_api->fb_fill_rect_blend(4, TASKBAR_PY+4, 80, 1, 0x60FFFFFF);
            g_api->fb_draw_string_px("\x0F RitOS", C_TEXT, 0, 12, TASKBAR_PY+12, 1);

            /* Separator */
            g_api->fb_fill_rect(88, TASKBAR_PY+6, 1, 28, 0xFF333360u);

            /* Window buttons */
            int bx = 96;
            int wc = d->get_window_count();
            for (int w = 0; w < wc; w++) {
                if (!d->is_window_visible(w)) continue;
                const char* lbl = short_label(d->get_window_title(w));
                int llen=0; while(lbl[llen])llen++;
                int bw = llen*8 + 16;
                if (bx+bw > 870) break;

                bool act  = d->is_window_active(w)!=0;
                bool hov  = (my>=TASKBAR_ROW && mx_px>=bx && mx_px<bx+bw);
                bool mini = d->is_window_minimized(w)!=0;

                uint32_t bc = hov ? C_BTN_HOV : (act ? C_BTN_ACT : C_BTN_NORM);
                g_api->fb_fill_rounded_rect(bx, TASKBAR_PY+4, bw, 32, 4, bc);
                if (act) g_api->fb_draw_hline_px(bx+2, TASKBAR_PY+35, bw-4, C_ACCENT);
                uint32_t lc = mini ? C_TEXT_DIM : (act ? C_TEXT : C_TEXT_DIM);
                g_api->fb_draw_string_px(lbl, lc, 0, bx+8, TASKBAR_PY+12, 1);
                bx += bw + 4;
            }

            /* Clock (right-aligned) */
            int h=0,m=0,s=0; g_api->get_time(&h,&m,&s);
            char tb[9];
            tb[0]=(h/10)+'0'; tb[1]=(h%10)+'0'; tb[2]=':';
            tb[3]=(m/10)+'0'; tb[4]=(m%10)+'0'; tb[5]=':';
            tb[6]=(s/10)+'0'; tb[7]=(s%10)+'0'; tb[8]='\0';
            g_api->fb_fill_rect(880, TASKBAR_PY+4, 136, 32, C_BTN_ACT);
            g_api->fb_draw_string_px(tb, C_CLOCK, 0, 896, TASKBAR_PY+12, 1);
        } else {
            /* VGA fallback */
            for (int x = 0; x < 128; x++) g_api->draw_char(' ', 15, 1, x, 47);
            const char* start = "\x0F RitOS ";
            for (int x=0; start[x]; x++) g_api->draw_char(start[x], 15, 3, 1+x, 47);
            int h=0,m=0,s=0; g_api->get_time(&h,&m,&s);
            char tb[9];
            tb[0]=(h/10)+'0';tb[1]=(h%10)+'0';tb[2]=':';
            tb[3]=(m/10)+'0';tb[4]=(m%10)+'0';tb[5]=':';
            tb[6]=(s/10)+'0';tb[7]=(s%10)+'0';tb[8]='\0';
            for(int x=0;tb[x];x++) g_api->draw_char(tb[x],10,1,118+x,47);
        }
    };

    mod.handle_click = [](void* inst, int mx, int my) {
        Desktop_Interface* d = static_cast<Desktop_Interface*>(inst);
        if (my < TASKBAR_ROW) return;

        int mx_px = mx * 8;

        /* Start button: pixel x 4..83 */
        if (mx_px >= 4 && mx_px < 84) {
            d->set_start_menu_open(!d->is_start_menu_open());
            return;
        }

        /* Window buttons starting at pixel 96 */
        int bx = 96;
        int wc = d->get_window_count();
        for (int w = 0; w < wc; w++) {
            if (!d->is_window_visible(w)) continue;
            const char* lbl = short_label(d->get_window_title(w));
            int llen=0; while(lbl[llen])llen++;
            int bw = llen*8 + 16;
            if (bx+bw > 870) break;
            if (mx_px >= bx && mx_px < bx+bw) { d->toggle_window(w); return; }
            bx += bw + 4;
        }
    };

    mod.handle_key = [](void* inst, char key) { (void)inst; (void)key; };
    return &mod;
}

#include <ritos/fluent.hpp>
#include <rit/system.hpp>
#include <rit/rbx_module.h>

extern const RitOS_API* g_api;

/*
 * Windows-11-style taskbar: 48px light acrylic strip at the bottom.
 * Start button stays LEFT-aligned (per user preference), followed by
 * icon-only buttons for every open window (accent underline = active).
 * Clock + date sit at the right edge. All hit testing is pixel-exact.
 */

#define BTN_W      40
#define BTN_H      40
#define BTN_GAP    4
#define START_X    8
#define WINBTN_X0  (START_X + BTN_W + BTN_GAP + 8)
#define CLOCK_W    96

static int tb_y() { return g_api->fb_get_height() - fluent::TASKBAR_H; }

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
            int sw = g_api->fb_get_width();
            int ty = tb_y();

            /* Acrylic strip: translucent mica over the wallpaper + top stroke */
            g_api->fb_fill_rect_blend(0, ty, sw, fluent::TASKBAR_H, fluent::TASKBAR_BG);
            g_api->fb_draw_hline_px(0, ty, sw, 0x30FFFFFFu);

            int mx = d->get_mouse_px_x();
            int my = d->get_mouse_px_y();
            bool row = (my >= ty);

            /* Start button: Windows logo, left-aligned */
            int by = ty + (fluent::TASKBAR_H - BTN_H) / 2;
            bool start_hov = row && mx >= START_X && mx < START_X + BTN_W;
            if (start_hov || d->is_start_menu_open())
                fluent::rrect(START_X, by, BTN_W, BTN_H, 4,
                              d->is_start_menu_open() ? fluent::PRESSED : fluent::HOVER);
            fluent::glyph_logo(START_X + (BTN_W - 20) / 2, by + (BTN_H - 20) / 2, 20);

            /* Window buttons: icon-only, 40x40 */
            int bx = WINBTN_X0;
            int wc = d->get_window_count();
            int max_x = sw - CLOCK_W - BTN_W;
            for (int w = 0; w < wc; w++) {
                if (!d->is_window_visible(w)) continue;
                if (bx + BTN_W > max_x) break;

                bool act  = d->is_window_active(w) != 0;
                bool mini = d->is_window_minimized(w) != 0;
                bool hov  = row && mx >= bx && mx < bx + BTN_W;

                if (hov)      fluent::rrect(bx, by, BTN_W, BTN_H, 4, fluent::HOVER);
                else if (act && !mini)
                              fluent::rrect(bx, by, BTN_W, BTN_H, 4, 0x28FFFFFFu);

                const unsigned int* icon = d->get_window_icon ? d->get_window_icon(w) : 0;
                if (icon) {
                    g_api->fb_blit_argb((const uint32_t*)icon,
                                        bx + (BTN_W - 32) / 2, by + (BTN_H - 32) / 2, 32, 32);
                } else {
                    const char* t = d->get_window_title(w);
                    char ch[2] = { t && t[0] ? t[0] : '?', 0 };
                    fluent::rrect(bx + 8, by + 8, 24, 24, 5, fluent::ACCENT);
                    fluent::text(ch, fluent::TEXT_WHITE, bx + 16, by + 12);
                }

                /* Open indicator: pill under the icon (wider when active) */
                int pw = (act && !mini) ? 16 : 6;
                fluent::rrect(bx + (BTN_W - pw) / 2, ty + fluent::TASKBAR_H - 4, pw, 3, 1,
                              (act && !mini) ? fluent::ACCENT : 0xFF8A8A8Au);
                bx += BTN_W + BTN_GAP;
            }

            /* Clock + date, right-aligned */
            int h=0,m=0,s=0; g_api->get_time(&h,&m,&s);
            int yr=0,mo=0,dy=0; g_api->get_date(&yr,&mo,&dy);
            if (yr < 100) yr += 2000;   /* RTC reports a 2-digit year */
            char tb[6], db[11];
            tb[0]=(h/10)+'0'; tb[1]=(h%10)+'0'; tb[2]=':';
            tb[3]=(m/10)+'0'; tb[4]=(m%10)+'0'; tb[5]='\0';
            db[0]=(dy/10)+'0'; db[1]=(dy%10)+'0'; db[2]='/';
            db[3]=(mo/10)+'0'; db[4]=(mo%10)+'0'; db[5]='/';
            db[6]=((yr/1000)%10)+'0'; db[7]=((yr/100)%10)+'0';
            db[8]=((yr/10)%10)+'0';   db[9]=(yr%10)+'0'; db[10]='\0';
            int cx = sw - CLOCK_W;
            bool clk_hov = row && mx >= cx;
            if (clk_hov) fluent::rrect(cx - 4, by, CLOCK_W, BTN_H, 4, fluent::HOVER);
            fluent::text_centered(tb, fluent::TEXT, cx - 4, CLOCK_W, ty + 7,
                                  fluent::FONT_SMALL);
            fluent::text_centered(db, fluent::TEXT, cx - 4, CLOCK_W, ty + 25,
                                  fluent::FONT_SMALL);
        } else {
            /* VGA fallback */
            for (int x = 0; x < 80; x++) g_api->draw_char(' ', 15, 1, x, 24);
            const char* start = " Start ";
            for (int x=0; start[x]; x++) g_api->draw_char(start[x], 15, 3, x, 24);
            int h=0,m=0,s=0; g_api->get_time(&h,&m,&s);
            char tb[9];
            tb[0]=(h/10)+'0';tb[1]=(h%10)+'0';tb[2]=':';
            tb[3]=(m/10)+'0';tb[4]=(m%10)+'0';tb[5]=':';
            tb[6]=(s/10)+'0';tb[7]=(s%10)+'0';tb[8]='\0';
            for(int x=0;tb[x];x++) g_api->draw_char(tb[x],10,1,71+x,24);
        }
    };

    /* mx/my are exact pixel coordinates */
    mod.handle_click = [](void* inst, int mx, int my) {
        Desktop_Interface* d = static_cast<Desktop_Interface*>(inst);
        if (!g_api->fb_is_available()) {
            if (mx < 8) d->set_start_menu_open(!d->is_start_menu_open());
            return;
        }
        if (my < tb_y()) return;

        /* Start button */
        if (mx >= START_X && mx < START_X + BTN_W) {
            d->set_start_menu_open(!d->is_start_menu_open());
            return;
        }

        /* Window buttons */
        int sw = g_api->fb_get_width();
        int bx = WINBTN_X0;
        int wc = d->get_window_count();
        int max_x = sw - CLOCK_W - BTN_W;
        for (int w = 0; w < wc; w++) {
            if (!d->is_window_visible(w)) continue;
            if (bx + BTN_W > max_x) break;
            if (mx >= bx && mx < bx + BTN_W) { d->toggle_window(w); return; }
            bx += BTN_W + BTN_GAP;
        }
    };

    mod.handle_key = [](void* inst, char key) { (void)inst; (void)key; };
    return &mod;
}

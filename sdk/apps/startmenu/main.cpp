#include "../../gui/include/ritos/window.hpp"
#include "../../framework/include/rit/system.hpp"
#include "../../framework/include/rit/rbx_module.h"

extern const RitOS_API* g_api;

/* Start menu: left side popup above taskbar */
#define SM_PX   4      /* pixel x */
#define SM_PY   424    /* pixel y (desktop area, above taskbar) */
#define SM_PW   168    /* pixel width */
#define SM_PH   300    /* pixel height */
#define SM_CX   1      /* char col */
#define SM_CY   27     /* char row (pixel 424/16=26.5→27) */
#define SM_CW   21     /* char width */
#define SM_CH   19     /* char height */

#define C_SM_BG1    0xFF112040u  /* deep navy bg */
#define C_SM_BG2    0xFF0D1830u
#define C_SM_HDR1   0xFF1C3060u  /* richer header */
#define C_SM_HDR2   0xFF142448u
#define C_SM_BORDER 0xFF3A6AB0u  /* vivid blue border */
#define C_SM_HOV    0xFF1E3870u  /* hover highlight */
#define C_SM_SEP    0xFF264A88u
#define C_ACCENT    0xFF5B8FFFu
#define C_TEXT      0xFFECF0FFu
#define C_TEXT_DIM  0xFF7A96C4u
#define C_DANGER    0xFFFF5F57u

static const char* const sm_items[] = {
    "System Monitor", "Calculator", "Text Editor",
    "File Explorer",  "Clock",      "Calendar",
    "Settings",       "Terminal",   "",          /* separator */
    "Restart",        "Shut Down"
};
#define SM_ITEM_COUNT 11

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
    g_api = api;
    static rbx_module mod;
    mod.type = RBX_MODULE_STARTMENU;
    const char* nm = "Start Menu"; int i=0;
    while(nm[i]&&i<31){mod.name[i]=nm[i];i++;} mod.name[i]='\0';
    mod.instance = (void*)desktop;

    mod.draw = [](void* inst) {
        const Desktop_Interface* d = static_cast<const Desktop_Interface*>(inst);
        int mx_px = d->get_mouse_x() * 8;
        int my_px = d->get_mouse_y() * 16;

        if (g_api->fb_is_available()) {
            /* Shadow */
            g_api->fb_fill_rect_blend(SM_PX+4, SM_PY+4, SM_PW, SM_PH, 0x80000000u);

            /* Background */
            g_api->fb_fill_rounded_rect(SM_PX, SM_PY, SM_PW, SM_PH, 6, C_SM_BG1);
            g_api->fb_fill_grad_v(SM_PX, SM_PY, SM_PW, 28, C_SM_HDR1, C_SM_HDR2);

            /* Border */
            g_api->fb_fill_rounded_rect(SM_PX-1, SM_PY-1, SM_PW+2, SM_PH+2, 6, C_SM_BORDER);
            g_api->fb_fill_rounded_rect(SM_PX, SM_PY, SM_PW, SM_PH, 6, C_SM_BG1);
            g_api->fb_fill_grad_v(SM_PX+1, SM_PY+1, SM_PW-2, 26, C_SM_HDR1, C_SM_HDR2);

            /* Header title */
            g_api->fb_draw_string_px("\x0F RitOS", C_TEXT, 0, SM_PX+12, SM_PY+7, 1);
            g_api->fb_draw_hline_px(SM_PX+1, SM_PY+28, SM_PW-2, C_SM_BORDER);

            /* Menu items */
            int item_h = 24;
            for (int i=0; i<SM_ITEM_COUNT; i++) {
                int iy = SM_PY + 32 + i * item_h;
                if (iy + item_h > SM_PY + SM_PH - 4) break;

                if (sm_items[i][0]=='\0') {
                    /* Separator */
                    g_api->fb_fill_rect(SM_PX+4, iy+11, SM_PW-8, 1, C_SM_SEP);
                    continue;
                }

                bool hov = (mx_px >= SM_PX && mx_px < SM_PX+SM_PW &&
                            my_px >= iy && my_px < iy+item_h);
                if (hov) g_api->fb_fill_rounded_rect(SM_PX+2, iy, SM_PW-4, item_h, 3, C_SM_HOV);

                uint32_t ic = (i >= 9) ? C_DANGER : C_ACCENT;
                if (hov) ic = C_TEXT;
                g_api->fb_draw_string_px("\x10", ic, 0, SM_PX+6, iy+4, 1);
                g_api->fb_draw_string_px(sm_items[i], C_TEXT, 0, SM_PX+18, iy+4, 1);
            }
        } else {
            /* VGA fallback */
            uint8_t dbg = (uint8_t)rit::Color::DarkGrey;
            uint8_t lcy = (uint8_t)rit::Color::LightCyan;
            for (int row=0; row<SM_CH; row++) {
                for (int col=0; col<SM_CW; col++)
                    g_api->draw_char(' ', 15, dbg, SM_CX+col, SM_CY+row);
            }
            for (int i=0; i<SM_ITEM_COUNT && i<SM_CH-2; i++) {
                if(sm_items[i][0]=='\0'){
                    for(int x=1;x<SM_CW-1;x++) g_api->draw_char('\xC4',lcy,dbg,SM_CX+x,SM_CY+1+i);
                    continue;
                }
                g_api->draw_char('\x10', lcy, dbg, SM_CX+1, SM_CY+1+i);
                for (int c=0; sm_items[i][c]; c++)
                    g_api->draw_char(sm_items[i][c], 15, dbg, SM_CX+3+c, SM_CY+1+i);
            }
        }
    };

    mod.handle_click = [](void* inst, int mx, int my) {
        Desktop_Interface* d = static_cast<Desktop_Interface*>(inst);
        d->set_start_menu_open(0);

        if (g_api->fb_is_available()) {
            int mx_px = mx * 8;
            int my_px = my * 16;
            int item_h = 24;
            for (int i=0; i<SM_ITEM_COUNT; i++) {
                int iy = SM_PY + 32 + i * item_h;
                if (sm_items[i][0]=='\0') continue;
                if (mx_px>=SM_PX && mx_px<SM_PX+SM_PW && my_px>=iy && my_px<iy+item_h) {
                    if (i==9) g_api->reboot();
                    else if (i==10) g_api->shutdown();
                    else d->launch_app(sm_items[i]);
                    return;
                }
            }
        } else {
            if (mx>=SM_CX && mx<SM_CX+SM_CW && my>=SM_CY+1 && my<SM_CY+SM_CH-1) {
                int idx = my - (SM_CY+1);
                if (idx>=0 && idx<8) d->launch_app(sm_items[idx]);
                else if (idx==9)  g_api->reboot();
                else if (idx==10) g_api->shutdown();
            }
        }
    };

    mod.handle_key = [](void* inst, char key) { (void)inst; (void)key; };
    return &mod;
}

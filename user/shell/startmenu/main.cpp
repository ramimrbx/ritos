#include <ritos/window.hpp>
#include <rit/system.hpp>
#include <rit/rbx_module.h>
#include <rit/rbx_format.h>

extern const RitOS_API* g_api;

/* Start menu: left side popup above taskbar */
#define SM_PX   4      /* pixel x */
#define SM_PY   392    /* pixel y (desktop area, above taskbar) */
#define SM_PW   168    /* pixel width */
#define SM_PH   332    /* pixel height */
#define SM_CX   1      /* char col */
#define SM_CY   25     /* char row (pixel 392/16=24.5→25) */
#define SM_CW   21     /* char width */
#define SM_CH   20     /* char height */

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

/* Menu entries are /launcher/*.stct shortcut files: the label is the
 * shortcut filename (extension hidden), the icon comes embedded in the
 * target .rbx (downsampled 32->16). Restart/Shut Down stay built in. */
#define SM_MAX_ITEMS 12

struct SmItem {
    char     label[24];
    char     target[64];
    uint32_t icon16[16 * 16];
    bool     has_icon;
};
static SmItem sm_items[SM_MAX_ITEMS];
static int    sm_count = 0;

static void sm_load_items() {
    sm_count = 0;
    const char* files[64];
    int n = g_api->vfs_get_file_list(files, 64);
    for (int i = 0; i < n && sm_count < SM_MAX_ITEMS; i++) {
        const char* f = files[i];
        const char* pre = "/launcher/";
        int p = 0; while (pre[p] && f[p] == pre[p]) p++;
        if (pre[p]) continue;
        int fl = 0; while (f[fl]) fl++;
        if (fl < 6 || f[fl-5]!='.' || f[fl-4]!='s' || f[fl-3]!='t' || f[fl-2]!='c' || f[fl-1]!='t')
            continue;

        SmItem& it = sm_items[sm_count];
        int li = 0;
        for (int k = p; k < fl - 5 && li < 23; k++) it.label[li++] = f[k];
        it.label[li] = '\0';

        int clen = 0;
        const char* c = g_api->vfs_read_file(f, &clen);
        if (!c || clen <= 0) continue;
        int ti = 0;
        while (ti < clen && ti < 63 && c[ti] && c[ti] != '\n' && c[ti] != '\r') {
            it.target[ti] = c[ti]; ti++;
        }
        it.target[ti] = '\0';

        it.has_icon = false;
        int len = 0;
        const char* data = g_api->vfs_read_file(it.target, &len);
        if (data && len >= (int)sizeof(rbx_header_t)) {
            const rbx_header_t* h = (const rbx_header_t*)data;
            if (h->magic[0]=='R' && h->magic[1]=='B' && h->magic[2]=='X' && h->magic[3]=='2' &&
                h->icon_size == RBX_ICON_BYTES &&
                h->icon_offset + h->icon_size <= (uint32_t)len) {
                const uint32_t* px32 = (const uint32_t*)(data + h->icon_offset);
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++)
                        it.icon16[y*16 + x] = px32[(y*2)*32 + (x*2)];
                it.has_icon = true;
            }
        }
        sm_count++;
    }
}

/* Row layout: 0..sm_count-1 = apps, sm_count = separator,
 * sm_count+1 = Restart, sm_count+2 = Shut Down */

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
    g_api = api;
    sm_load_items();
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
            int total = sm_count + 3;   /* apps + separator + restart + shutdown */
            for (int i=0; i<total; i++) {
                int iy = SM_PY + 32 + i * item_h;
                if (iy + item_h > SM_PY + SM_PH - 4) break;

                if (i == sm_count) {
                    /* Separator */
                    g_api->fb_fill_rect(SM_PX+4, iy+11, SM_PW-8, 1, C_SM_SEP);
                    continue;
                }

                bool hov = (mx_px >= SM_PX && mx_px < SM_PX+SM_PW &&
                            my_px >= iy && my_px < iy+item_h);
                if (hov) g_api->fb_fill_rounded_rect(SM_PX+2, iy, SM_PW-4, item_h, 3, C_SM_HOV);

                if (i < sm_count) {
                    if (sm_items[i].has_icon)
                        g_api->fb_blit_argb(sm_items[i].icon16, SM_PX+6, iy+4, 16, 16);
                    else
                        g_api->fb_draw_string_px("\x10", hov ? C_TEXT : C_ACCENT, 0, SM_PX+6, iy+4, 1);
                    g_api->fb_draw_string_px(sm_items[i].label, C_TEXT, 0, SM_PX+26, iy+4, 1);
                } else {
                    const char* lbl = (i == sm_count+1) ? "Restart" : "Shut Down";
                    g_api->fb_draw_string_px("\x10", hov ? C_TEXT : C_DANGER, 0, SM_PX+6, iy+4, 1);
                    g_api->fb_draw_string_px(lbl, C_TEXT, 0, SM_PX+26, iy+4, 1);
                }
            }
        } else {
            /* VGA fallback */
            uint8_t dbg = (uint8_t)rit::Color::DarkGrey;
            uint8_t lcy = (uint8_t)rit::Color::LightCyan;
            for (int row=0; row<SM_CH; row++) {
                for (int col=0; col<SM_CW; col++)
                    g_api->draw_char(' ', 15, dbg, SM_CX+col, SM_CY+row);
            }
            int total = sm_count + 3;
            for (int i=0; i<total && i<SM_CH-2; i++) {
                if (i == sm_count) {
                    for(int x=1;x<SM_CW-1;x++) g_api->draw_char('\xC4',lcy,dbg,SM_CX+x,SM_CY+1+i);
                    continue;
                }
                const char* lbl = (i < sm_count) ? sm_items[i].label
                                : (i == sm_count+1) ? "Restart" : "Shut Down";
                g_api->draw_char('\x10', lcy, dbg, SM_CX+1, SM_CY+1+i);
                for (int c=0; lbl[c]; c++)
                    g_api->draw_char(lbl[c], 15, dbg, SM_CX+3+c, SM_CY+1+i);
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
            int total = sm_count + 3;
            for (int i=0; i<total; i++) {
                int iy = SM_PY + 32 + i * item_h;
                if (i == sm_count) continue; /* separator */
                if (mx_px>=SM_PX && mx_px<SM_PX+SM_PW && my_px>=iy && my_px<iy+item_h) {
                    if (i == sm_count+1)      g_api->reboot();
                    else if (i == sm_count+2) g_api->shutdown();
                    else                      d->launch_app(sm_items[i].target);
                    return;
                }
            }
        } else {
            if (mx>=SM_CX && mx<SM_CX+SM_CW && my>=SM_CY+1 && my<SM_CY+SM_CH-1) {
                int idx = my - (SM_CY+1);
                if (idx>=0 && idx<sm_count) d->launch_app(sm_items[idx].target);
                else if (idx==sm_count+1)   g_api->reboot();
                else if (idx==sm_count+2)   g_api->shutdown();
            }
        }
    };

    mod.handle_key = [](void* inst, char key) { (void)inst; (void)key; };
    return &mod;
}

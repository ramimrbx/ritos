#include <ritos/window.hpp>
#include <rit/system.hpp>
#include <rit/rbx_module.h>

extern const RitOS_API* g_api;

#define C_TOP1    0xF20A1526u  /* dark midnight */
#define C_TOP2    0xF2101E38u
#define C_ACCENT  0xFF5B8FFFu  /* vivid blue */
#define C_TEXT    0xFFECF0FFu
#define C_DIM     0xFF7A96C4u
#define C_SEP     0xFF2A4068u
#define TOPBAR_H  16   /* 1 char row = 16px */

static void int_to_str(int n, char* buf) {
    int i=0; bool neg=n<0; if(neg)n=-n;
    if(n==0){buf[i++]='0';}
    else{while(n>0){buf[i++]=(n%10)+'0';n/=10;}}
    if(neg)buf[i++]='-'; buf[i]='\0';
    int len=i;
    for(int j=0;j<len/2;j++){char t=buf[j];buf[j]=buf[len-j-1];buf[len-j-1]=t;}
}
static int str_len(const char* s){int i=0;while(s[i])i++;return i;}

extern "C" rbx_module* rbx_module_init(const RitOS_API* api, const Desktop_Interface* desktop) {
    g_api = api;
    static rbx_module mod;
    mod.type = RBX_MODULE_STATUSBAR;
    const char* nm = "Status Bar"; int i=0;
    while(nm[i]&&i<31){mod.name[i]=nm[i];i++;} mod.name[i]='\0';
    mod.instance = (void*)desktop;

    mod.draw = [](void* inst) {
        const Desktop_Interface* d = static_cast<const Desktop_Interface*>(inst);

        if (g_api->fb_is_available()) {
            /* ── Pixel top bar ── */
            g_api->fb_fill_grad_v(0, 0, 1024, TOPBAR_H, C_TOP1, C_TOP2);

            /* Logo */
            g_api->fb_draw_string_px("\x0F", C_ACCENT, 0, 4, 0, 1);
            g_api->fb_fill_rect(14, 3, 1, 10, C_SEP);

            /* Shortcuts */
            g_api->fb_draw_string_px("Tab", C_ACCENT, 0, 20, 0, 1);
            g_api->fb_draw_string_px(" Cycle", C_DIM, 0, 44, 0, 1);
            g_api->fb_fill_rect(100, 3, 1, 10, C_SEP);
            g_api->fb_draw_string_px("Esc", C_ACCENT, 0, 106, 0, 1);
            g_api->fb_draw_string_px(" Close", C_DIM, 0, 130, 0, 1);
            g_api->fb_fill_rect(190, 3, 1, 10, C_SEP);

            /* Active window */
            g_api->fb_draw_string_px("Active:", C_ACCENT, 0, 196, 0, 1);
            const char* at = "None";
            int wc = d->get_window_count();
            for (int w=0; w<wc; w++) { if(d->is_window_active(w)){at=d->get_window_title(w);break;} }
            g_api->fb_draw_string_px(at, C_TEXT, 0, 252, 0, 1);

            /* Memory (right-aligned) */
            size_t heap = g_api->get_heap_usage();
            char hbuf[16]; int_to_str((int)heap, hbuf);
            int hw = str_len(hbuf);
            int rx = 1024 - 4 - (hw+2)*8;
            g_api->fb_draw_string_px("Mem:", C_ACCENT, 0, rx-32, 0, 1);
            g_api->fb_draw_string_px(hbuf, C_TEXT, 0, rx, 0, 1);
            g_api->fb_draw_string_px("B", C_DIM, 0, rx+hw*8, 0, 1);
        } else {
            /* VGA fallback: draw at row 0 */
            for (int x=0;x<128;x++) g_api->draw_char(' ',15,8,x,0);
            g_api->draw_char('\x0F',10,8,1,0);
            g_api->draw_char('\xB3',11,8,3,0);
            const char* k1=" Tab Cycle  ";
            for(int x=0;k1[x];x++) g_api->draw_char(k1[x],11,8,4+x,0);
            g_api->draw_char('\xB3',11,8,16,0);
            const char* k2=" Esc Close  ";
            for(int x=0;k2[x];x++) g_api->draw_char(k2[x],11,8,17+x,0);
        }
    };

    mod.handle_click = [](void* inst, int mx, int my) { (void)inst;(void)mx;(void)my; };
    mod.handle_key   = [](void* inst, char key)       { (void)inst;(void)key; };
    return &mod;
}

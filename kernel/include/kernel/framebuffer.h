#ifndef KERNEL_FB_H
#define KERNEL_FB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize framebuffer from multiboot info fields */
void fb_init(uint64_t phys_addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp);

int  fb_is_available(void);
int  fb_get_width(void);
int  fb_get_height(void);

/* ─── Primitive drawing (pixel coordinates) ─────────────────────────── */
void fb_set_pixel(int x, int y, uint32_t argb);
void fb_fill_rect(int x, int y, int w, int h, uint32_t argb);
void fb_fill_rect_blend(int x, int y, int w, int h, uint32_t argb);
void fb_blit_argb(const uint32_t* src, int x, int y, int w, int h);
/* Nearest-neighbour scaled blit (alpha-blended), sw*sh source -> dw*dh dest */
void fb_blit_argb_scaled(const uint32_t* src, int sw, int sh,
                         int dx, int dy, int dw, int dh);
void fb_draw_hline(int x, int y, int w, uint32_t argb);
void fb_draw_vline(int x, int y, int h, uint32_t argb);

/* ─── Gradients ──────────────────────────────────────────────────────── */
void fb_fill_grad_v(int x, int y, int w, int h, uint32_t top, uint32_t bot);
void fb_fill_grad_h(int x, int y, int w, int h, uint32_t left, uint32_t right);

/* ─── Shapes ─────────────────────────────────────────────────────────── */
void fb_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t argb);
void fb_fill_circle(int cx, int cy, int r, uint32_t argb);

/* ─── Text rendering (8×16 bitmap font) ─────────────────────────────── */
/* x,y are pixel coordinates; transparent_bg=1 skips drawing background */
void fb_draw_char(char c, uint32_t fg, uint32_t bg, int x, int y, int transparent_bg);
void fb_draw_string(const char* s, uint32_t fg, uint32_t bg, int x, int y, int transparent_bg);
int  fb_string_pixel_width(const char* s);  /* Returns pixel width */
/* Integer-scaled text (always transparent background), for headings */
void fb_draw_string_scaled(const char* s, uint32_t fg, int x, int y, int scale);

/* ─── Wallpaper (embedded at build time, scaled to cover the screen) ─── */
/* Draws the wallpaper over the whole back buffer; 0 if none embedded */
int  fb_draw_wallpaper(void);

/* draw_char mapped to VGA char-grid coords (col*8, row*16) – compat shim */
void fb_draw_char_grid(char c, uint8_t vga_fg, uint8_t vga_bg, int col, int row);

/* ─── Double-buffering ───────────────────────────────────────────────── */
void fb_clear_back(uint32_t argb);
void fb_flush(void);

/* ─── Colour helpers ─────────────────────────────────────────────────── */
static inline uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static inline uint32_t fb_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static inline uint32_t fb_blend_pixel(uint32_t dst, uint32_t src) {
    uint32_t sa = (src >> 24) & 0xFF;
    if (sa == 0)   return dst;
    if (sa == 255) return (src & 0x00FFFFFF) | 0xFF000000u;
    uint32_t da = 255 - sa;
    uint32_t r = ((src >> 16 & 0xFF) * sa + (dst >> 16 & 0xFF) * da) / 255;
    uint32_t g = ((src >>  8 & 0xFF) * sa + (dst >>  8 & 0xFF) * da) / 255;
    uint32_t b = ((src       & 0xFF) * sa + (dst       & 0xFF) * da) / 255;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

/* VGA 16-colour to 32-bit ARGB mapping */
extern const uint32_t g_vga_palette[16];

/* ─── Common colour constants ────────────────────────────────────────── */
#define FB_BLACK        0xFF000000u
#define FB_WHITE        0xFFFFFFFFu
/* Desktop gradient: deep indigo to midnight blue */
#define FB_DESKTOP_TOP  0xFF0E1628u
#define FB_DESKTOP_BOT  0xFF1A2240u
/* Bars */
#define FB_TASKBAR      0xF0111D36u
#define FB_TOPBAR       0xF00B1628u
/* Window decorations */
#define FB_WIN_TITLE1   0xFF1C2E54u
#define FB_WIN_TITLE2   0xFF14223Eu
#define FB_WIN_BODY     0xFFF4F6FFu
#define FB_WIN_BORDER   0xFF3A5A9Cu
/* Accent colours */
#define FB_ACCENT       0xFF5B8FFFu
#define FB_ACCENT2      0xFF3DCFC7u
/* Text */
#define FB_TEXT_LIGHT   0xFFECF0FFu
#define FB_TEXT_DIM     0xFF8BA4CCu
#define FB_TEXT_DARK    0xFF0D1628u
#define FB_TEXT_GREY    0xFF6E84A8u
/* Buttons */
#define FB_CLOSE_BTN    0xFFFF5F57u
#define FB_MIN_BTN      0xFFFEBC2Eu
#define FB_MAX_BTN      0xFF28C840u
/* Hover / selection */
#define FB_HOVER_BG     0xFF223460u
#define FB_SELECTED_BG  0xFF2A4280u
#define FB_SHADOW       0x80000000u

/* Layout constants */
#define FB_TASKBAR_H    48
#define FB_TITLEBAR_H   32
#define FB_ICON_SIZE    48

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_FB_H */

#include "../include/kernel/fb.h"
#include "../../assets/font/font_8x16.h"

extern void* kmalloc(size_t size);

/* ─── State ──────────────────────────────────────────────────────────── */
static uint32_t* g_fb    = 0;
static uint32_t* g_back  = 0;
static int       g_w     = 0;
static int       g_h     = 0;
static uint32_t  g_pitch = 0;
static int       g_bpp   = 0;
static int       g_ready = 0;

/* VGA palette */
const uint32_t g_vga_palette[16] = {
    0xFF000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA,
    0xFFAA0000, 0xFFAA00AA, 0xFFAA5500, 0xFFAAAAAA,
    0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF,
    0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF,
};

/* ─── Init ───────────────────────────────────────────────────────────── */
void fb_init(uint64_t phys_addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp) {
    if (bpp != 32 && bpp != 24) return;
    if (width == 0 || height == 0) return;
    g_fb    = (uint32_t*)(uint32_t)phys_addr;
    g_pitch = pitch;
    g_w     = (int)width;
    g_h     = (int)height;
    g_bpp   = bpp;
    size_t sz = (size_t)width * height * 4;
    g_back  = (uint32_t*)kmalloc(sz);
    if (!g_back) return;
    for (size_t i = 0; i < (size_t)width * height; i++) g_back[i] = 0xFF000000u;
    g_ready = 1;
}

int fb_is_available(void) { return g_ready; }
int fb_get_width(void)    { return g_w; }
int fb_get_height(void)   { return g_h; }

/* ─── Internal back-buffer helpers ──────────────────────────────────── */
static inline void bp(int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)g_w && (unsigned)y < (unsigned)g_h)
        g_back[y * g_w + x] = c;
}
static inline uint32_t bg(int x, int y) {
    if ((unsigned)x < (unsigned)g_w && (unsigned)y < (unsigned)g_h)
        return g_back[y * g_w + x];
    return 0;
}

/* ─── Primitives ─────────────────────────────────────────────────────── */
void fb_set_pixel(int x, int y, uint32_t argb) {
    if (!g_ready) return;
    bp(x, y, argb | 0xFF000000u);
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t argb) {
    if (!g_ready) return;
    uint32_t c = argb | 0xFF000000u;
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = x + w; if (x2 > g_w) x2 = g_w;
    int y2 = y + h; if (y2 > g_h) y2 = g_h;
    for (int py = y1; py < y2; py++) {
        uint32_t* row = g_back + py * g_w;
        for (int px = x1; px < x2; px++) row[px] = c;
    }
}

void fb_fill_rect_blend(int x, int y, int w, int h, uint32_t argb) {
    if (!g_ready) return;
    uint8_t a = (argb >> 24) & 0xFF;
    if (a == 255) { fb_fill_rect(x, y, w, h, argb); return; }
    if (a == 0) return;
    int x1 = x < 0 ? 0 : x, y1 = y < 0 ? 0 : y;
    int x2 = x+w; if (x2 > g_w) x2 = g_w;
    int y2 = y+h; if (y2 > g_h) y2 = g_h;
    for (int py = y1; py < y2; py++) {
        uint32_t* row = g_back + py * g_w;
        for (int px = x1; px < x2; px++)
            row[px] = fb_blend_pixel(row[px], argb);
    }
}

void fb_blit_argb(const uint32_t* src, int x, int y, int w, int h) {
    if (!g_ready || !src) return;
    for (int sy = 0; sy < h; sy++) {
        int dy = y + sy;
        if (dy < 0 || dy >= g_h) continue;
        for (int sx = 0; sx < w; sx++) {
            int dx = x + sx;
            if (dx < 0 || dx >= g_w) continue;
            uint32_t p = src[sy * w + sx];
            uint8_t pa = (p >> 24) & 0xFF;
            if (pa == 0) continue;
            g_back[dy * g_w + dx] = (pa == 255) ? (p | 0xFF000000u) : fb_blend_pixel(g_back[dy * g_w + dx], p);
        }
    }
}

void fb_draw_hline(int x, int y, int w, uint32_t argb) {
    fb_fill_rect(x, y, w, 1, argb);
}
void fb_draw_vline(int x, int y, int h, uint32_t argb) {
    fb_fill_rect(x, y, 1, h, argb);
}

/* ─── Gradients ──────────────────────────────────────────────────────── */
void fb_fill_grad_v(int x, int y, int w, int h, uint32_t top, uint32_t bot) {
    if (!g_ready || h <= 0) return;
    int r1=(top>>16)&0xFF, g1=(top>>8)&0xFF, b1=top&0xFF;
    int r2=(bot>>16)&0xFF, g2=(bot>>8)&0xFF, b2=bot&0xFF;
    int hm = h > 1 ? h-1 : 1;
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy; if (py < 0 || py >= g_h) continue;
        uint8_t r = (uint8_t)(r1 + (r2-r1)*dy/hm);
        uint8_t g = (uint8_t)(g1 + (g2-g1)*dy/hm);
        uint8_t b = (uint8_t)(b1 + (b2-b1)*dy/hm);
        uint32_t c = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b;
        int x1 = x<0?0:x, x2 = x+w; if (x2>g_w) x2=g_w;
        uint32_t* row = g_back + py * g_w;
        for (int px = x1; px < x2; px++) row[px] = c;
    }
}

void fb_fill_grad_h(int x, int y, int w, int h, uint32_t left, uint32_t right) {
    if (!g_ready || w <= 0) return;
    int r1=(left>>16)&0xFF, g1=(left>>8)&0xFF, b1=left&0xFF;
    int r2=(right>>16)&0xFF, g2=(right>>8)&0xFF, b2=right&0xFF;
    int wm = w > 1 ? w-1 : 1;
    for (int dx = 0; dx < w; dx++) {
        int px = x + dx; if (px < 0 || px >= g_w) continue;
        uint8_t r = (uint8_t)(r1 + (r2-r1)*dx/wm);
        uint8_t g = (uint8_t)(g1 + (g2-g1)*dx/wm);
        uint8_t b = (uint8_t)(b1 + (b2-b1)*dx/wm);
        uint32_t c = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b;
        int y1 = y<0?0:y, y2 = y+h; if (y2>g_h) y2=g_h;
        for (int py = y1; py < y2; py++) g_back[py * g_w + px] = c;
    }
}

/* ─── Shapes ─────────────────────────────────────────────────────────── */
void fb_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t argb) {
    if (!g_ready) return;
    if (r <= 0) { fb_fill_rect(x, y, w, h, argb); return; }
    uint32_t c = argb | 0xFF000000u;
    fb_fill_rect(x+r, y, w-2*r, h, c);
    fb_fill_rect(x, y+r, r, h-2*r, c);
    fb_fill_rect(x+w-r, y+r, r, h-2*r, c);
    for (int dy = 0; dy <= r; dy++) {
        for (int dx = 0; dx <= r; dx++) {
            if (dx*dx + dy*dy <= r*r) {
                bp(x+r-dx,     y+r-dy,     c);
                bp(x+w-r+dx-1, y+r-dy,     c);
                bp(x+r-dx,     y+h-r+dy-1, c);
                bp(x+w-r+dx-1, y+h-r+dy-1, c);
            }
        }
    }
}

void fb_fill_circle(int cx, int cy, int r, uint32_t argb) {
    if (!g_ready) return;
    uint32_t c = argb | 0xFF000000u;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy <= r*r) bp(cx+dx, cy+dy, c);
        }
    }
}

/* ─── Text ───────────────────────────────────────────────────────────── */
void fb_draw_char(char c, uint32_t fg, uint32_t bg, int x, int y, int transparent_bg) {
    if (!g_ready) return;
    uint32_t fgc = fg | 0xFF000000u;
    uint32_t bgc = bg | 0xFF000000u;
    const uint8_t* glyph = g_font_8x16[(unsigned char)c];
    for (int row = 0; row < 16; row++) {
        int py = y + row;
        if (py < 0 || py >= g_h) continue;
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            int px = x + col;
            if (px < 0 || px >= g_w) continue;
            if (bits & (0x80 >> col)) g_back[py * g_w + px] = fgc;
            else if (!transparent_bg) g_back[py * g_w + px] = bgc;
        }
    }
}

void fb_draw_string(const char* s, uint32_t fg, uint32_t bg, int x, int y, int transparent_bg) {
    if (!g_ready || !s) return;
    int cx = x;
    while (*s) {
        if (*s == '\n') { y += 16; cx = x; s++; continue; }
        fb_draw_char(*s, fg, bg, cx, y, transparent_bg);
        cx += 8; s++;
    }
}

int fb_string_pixel_width(const char* s) {
    int n = 0;
    while (*s) { if (*s != '\n') n++; s++; }
    return n * 8;
}

void fb_draw_char_grid(char c, uint8_t vga_fg, uint8_t vga_bg, int col, int row) {
    uint32_t fg = g_vga_palette[vga_fg & 0xF];
    uint32_t bg = g_vga_palette[vga_bg & 0xF];
    fb_draw_char(c, fg, bg, col * 8, row * 16, 0);
}

/* ─── Double-buffering ───────────────────────────────────────────────── */
void fb_clear_back(uint32_t argb) {
    if (!g_ready) return;
    uint32_t c = argb | 0xFF000000u;
    int n = g_w * g_h;
    for (int i = 0; i < n; i++) g_back[i] = c;
}

void fb_flush(void) {
    if (!g_ready || !g_back || !g_fb) return;
    for (int y = 0; y < g_h; y++) {
        uint32_t* src = g_back + y * g_w;
        uint32_t* dst = (uint32_t*)((uint8_t*)g_fb + (uint32_t)y * g_pitch);
        for (int x = 0; x < g_w; x++) dst[x] = src[x];
    }
}

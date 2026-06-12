#include <kernel/framebuffer.h>
#include <kernel/palette.h>
#include <kernel/string.h>
#include <font_8x16.h>
#include <instrument_font.h>

extern void* kmalloc(size_t size);

/* ─── State ──────────────────────────────────────────────────────────── */
static uint32_t* g_fb    = 0;
static uint32_t* g_back  = 0;
static int       g_w     = 0;
static int       g_h     = 0;
static uint32_t  g_pitch = 0;
static int       g_bpp   = 0;
static int       g_ready = 0;

/* Page flipping (when the port's display controller supports it): the
 * back buffer is copied to the hidden VRAM page and the scanout origin
 * flips to it atomically, so a frame can never be seen half-painted. */
typedef struct { int x0, y0, x1, y1, valid; } fbrect_t;
static int      g_flip = 0;       /* >=2 VRAM pages available        */
static int      g_page = 0;       /* page currently being scanned out */
static fbrect_t g_stale[2];       /* per-page region older than g_back */

/* Dirty bounding box: the only region fb_flush copies to video memory.
 * Every primitive unions its footprint into it. Cheap to maintain and
 * makes cursor-only frames ~1000x less memory traffic at 1080p. */
static int g_dx0, g_dy0, g_dx1, g_dy1;   /* inclusive-exclusive */
static int g_dirty = 0;

static void mark(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > g_w) x1 = g_w;
    if (y1 > g_h) y1 = g_h;
    if (x >= x1 || y >= y1) return;
    if (!g_dirty) {
        g_dx0 = x; g_dy0 = y; g_dx1 = x1; g_dy1 = y1;
        g_dirty = 1;
    } else {
        if (x  < g_dx0) g_dx0 = x;
        if (y  < g_dy0) g_dy0 = y;
        if (x1 > g_dx1) g_dx1 = x1;
        if (y1 > g_dy1) g_dy1 = y1;
    }
}

static void rect_union(fbrect_t* r, int x0, int y0, int x1, int y1) {
    if (!r->valid) {
        r->x0 = x0; r->y0 = y0; r->x1 = x1; r->y1 = y1; r->valid = 1;
        return;
    }
    if (x0 < r->x0) r->x0 = x0;
    if (y0 < r->y0) r->y0 = y0;
    if (x1 > r->x1) r->x1 = x1;
    if (y1 > r->y1) r->y1 = y1;
}

/* VGA palette, remapped to the modern RitOS theme */
const uint32_t g_vga_palette[16] = RITOS_PALETTE_INIT;

/* ─── Weak display-controller hooks (overridden by the port driver) ──── */
int __attribute__((weak)) fb_hw_flip_setup(uint64_t phys_addr, int w, int h,
                                           int bpp, uint32_t* pitch_io) {
    (void)phys_addr; (void)w; (void)h; (void)bpp; (void)pitch_io;
    return 0;
}
void __attribute__((weak)) fb_hw_flip(int y_offset) { (void)y_offset; }

/* ─── Init ───────────────────────────────────────────────────────────── */
void fb_init(uint64_t phys_addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp) {
    if (bpp != 32 && bpp != 24) return;
    if (width == 0 || height == 0) return;
    if (phys_addr + (uint64_t)pitch * height > 0xFFFFFFFFull) return; /* unreachable on i386 */
    g_fb    = (uint32_t*)(uint32_t)phys_addr;
    g_pitch = pitch;
    g_w     = (int)width;
    g_h     = (int)height;
    g_bpp   = bpp;
    size_t sz = (size_t)width * height * 4;
    g_back  = (uint32_t*)kmalloc(sz);
    if (!g_back) return;
    for (size_t i = 0; i < (size_t)width * height; i++) g_back[i] = 0xFF000000u;

    /* Ask the port's display controller for VRAM pages to flip between */
    if (bpp == 32 &&
        fb_hw_flip_setup(phys_addr, g_w, g_h, bpp, &g_pitch) >= 2) {
        g_flip = 1;
        g_page = 0;
        g_stale[0] = (fbrect_t){0, 0, g_w, g_h, 1};
        g_stale[1] = (fbrect_t){0, 0, g_w, g_h, 1};
    }

    g_ready = 1;
    mark(0, 0, g_w, g_h);
}

int fb_is_available(void) { return g_ready; }
int fb_get_width(void)    { return g_w; }
int fb_get_height(void)   { return g_h; }

/* ─── Internal back-buffer helpers ──────────────────────────────────── */
static inline void bp(int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)g_w && (unsigned)y < (unsigned)g_h)
        g_back[y * g_w + x] = c;
}

static inline void bp_blend(int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)g_w && (unsigned)y < (unsigned)g_h) {
        uint32_t* p = &g_back[y * g_w + x];
        *p = fb_blend_pixel(*p, c);
    }
}

/* Integer square root (32-bit), used for AA edge coverage */
static uint32_t isqrt32(uint32_t v) {
    uint32_t r = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return r;
}

/* ─── Primitives ─────────────────────────────────────────────────────── */
void fb_set_pixel(int x, int y, uint32_t argb) {
    if (!g_ready) return;
    bp(x, y, argb | 0xFF000000u);
    mark(x, y, 1, 1);
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
    mark(x, y, w, h);
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
    mark(x, y, w, h);
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
    mark(x, y, w, h);
}

/* Bilinear sample from an ARGB source at 16.16 fixed-point coordinates */
static uint32_t bilinear(const uint32_t* src, int sw, int sh,
                         uint32_t fx, uint32_t fy) {
    int x0 = (int)(fx >> 16), y0 = (int)(fy >> 16);
    if (x0 >= sw - 1) x0 = sw - 2;
    if (x0 < 0) x0 = 0;
    if (y0 >= sh - 1) y0 = sh - 2;
    if (y0 < 0) y0 = 0;
    uint32_t u = (fx >> 8) & 0xFF, v = (fy >> 8) & 0xFF;
    const uint32_t* r0 = src + y0 * sw + x0;
    const uint32_t* r1 = (sh > 1) ? r0 + sw : r0;
    uint32_t p00 = r0[0], p01 = (sw > 1) ? r0[1] : r0[0];
    uint32_t p10 = r1[0], p11 = (sw > 1) ? r1[1] : r1[0];
    uint32_t w00 = (256 - u) * (256 - v) >> 8, w01 = u * (256 - v) >> 8;
    uint32_t w10 = (256 - u) * v >> 8,         w11 = u * v >> 8;
    uint32_t out = 0;
    for (int sh8 = 0; sh8 < 32; sh8 += 8) {
        uint32_t c = ((p00 >> sh8 & 0xFF) * w00 + (p01 >> sh8 & 0xFF) * w01 +
                      (p10 >> sh8 & 0xFF) * w10 + (p11 >> sh8 & 0xFF) * w11) >> 8;
        out |= (c & 0xFF) << sh8;
    }
    return out;
}

void fb_blit_argb_scaled(const uint32_t* src, int sw, int sh,
                         int dx, int dy, int dw, int dh) {
    if (!g_ready || !src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    /* Edge-to-edge sampling so the first/last texels land on the borders */
    uint32_t sx_step = dw > 1 ? (uint32_t)((sw - 1) << 16) / (dw - 1) : 0;
    uint32_t sy_step = dh > 1 ? (uint32_t)((sh - 1) << 16) / (dh - 1) : 0;
    for (int oy = 0; oy < dh; oy++) {
        int py = dy + oy;
        if (py < 0 || py >= g_h) continue;
        uint32_t fy = (uint32_t)oy * sy_step;
        uint32_t* drow = g_back + py * g_w;
        uint32_t fx = 0;
        for (int ox = 0; ox < dw; ox++, fx += sx_step) {
            int px = dx + ox;
            if (px < 0 || px >= g_w) continue;
            uint32_t p = bilinear(src, sw, sh, fx, fy);
            uint8_t pa = (p >> 24) & 0xFF;
            if (pa == 0) continue;
            drow[px] = (pa == 255) ? (p | 0xFF000000u) : fb_blend_pixel(drow[px], p);
        }
    }
    mark(dx, dy, dw, dh);
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
    mark(x, y, w, h);
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
    mark(x, y, w, h);
}

/* ─── Anti-aliased shapes ────────────────────────────────────────────── */

/* Coverage (0..256) of the pixel whose centre is d8 (8.8 px) from the arc
 * centre, for a disc of radius r px: full inside, 1px AA ramp at the rim. */
static inline int arc_coverage(uint32_t d8, int r) {
    int edge = r * 256 + 128;
    int cov = edge - (int)d8;
    if (cov < 0) cov = 0;
    if (cov > 256) cov = 256;
    return cov;
}

/* Distance (8.8) from pixel (px,py) centre to point (cx,cy) in px units */
static inline uint32_t pixel_dist8(int px, int py, int cx, int cy) {
    int dx = cx * 256 - (px * 256 + 128);
    int dy = cy * 256 - (py * 256 + 128);
    return isqrt32((uint32_t)(dx * dx) + (uint32_t)(dy * dy));
}

static inline void blend_cov(int x, int y, uint32_t argb, uint32_t a, int cov) {
    if (cov <= 0) return;
    uint32_t sa = (a * (uint32_t)cov) >> 8;
    if (sa == 0) return;
    bp_blend(x, y, (sa << 24) | (argb & 0x00FFFFFF));
}

void fb_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t argb) {
    if (!g_ready || w <= 0 || h <= 0) return;
    uint32_t a = (argb >> 24) ? (argb >> 24) : 255;
    uint32_t fill = (a << 24) | (argb & 0x00FFFFFF);
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r <= 0) {
        if (a == 255) fb_fill_rect(x, y, w, h, fill);
        else fb_fill_rect_blend(x, y, w, h, fill);
        return;
    }

    /* Straight spans */
    if (a == 255) {
        fb_fill_rect(x + r, y, w - 2*r, h, fill);
        fb_fill_rect(x, y + r, r, h - 2*r, fill);
        fb_fill_rect(x + w - r, y + r, r, h - 2*r, fill);
    } else {
        fb_fill_rect_blend(x + r, y, w - 2*r, h, fill);
        fb_fill_rect_blend(x, y + r, r, h - 2*r, fill);
        fb_fill_rect_blend(x + w - r, y + r, r, h - 2*r, fill);
    }

    /* AA corners: arc centres sit r px inside each corner */
    int cxs[2] = { x + r, x + w - r };
    int cys[2] = { y + r, y + h - r };
    for (int cy = 0; cy < 2; cy++) {
        for (int cx = 0; cx < 2; cx++) {
            int bx = cx ? x + w - r : x;     /* corner box origin */
            int by = cy ? y + h - r : y;
            for (int py = by; py < by + r; py++) {
                for (int px = bx; px < bx + r; px++) {
                    uint32_t d8 = pixel_dist8(px, py, cxs[cx], cys[cy]);
                    int cov = arc_coverage(d8, r);
                    if (cov >= 256 && a == 255) bp(px, py, fill);
                    else blend_cov(px, py, argb, a, cov);
                }
            }
        }
    }
    mark(x, y, w, h);
}

void fb_stroke_rounded_rect(int x, int y, int w, int h, int r, uint32_t argb) {
    if (!g_ready || w <= 1 || h <= 1) return;
    uint32_t a = (argb >> 24) ? (argb >> 24) : 255;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r < 0) r = 0;

    /* Straight 1px edges between the corner arcs */
    uint32_t c = (a << 24) | (argb & 0x00FFFFFF);
    fb_fill_rect_blend(x + r, y, w - 2*r, 1, c);
    fb_fill_rect_blend(x + r, y + h - 1, w - 2*r, 1, c);
    fb_fill_rect_blend(x, y + r, 1, h - 2*r, c);
    fb_fill_rect_blend(x + w - 1, y + r, 1, h - 2*r, c);
    if (r <= 0) { mark(x, y, w, h); return; }

    /* AA ring segments at the corners: coverage peaks on the stroke path
     * (radius r - 0.5 from the arc centre) and falls off over 1px */
    int ring8 = r * 256 - 128;
    int cxs[2] = { x + r, x + w - r };
    int cys[2] = { y + r, y + h - r };
    for (int cy = 0; cy < 2; cy++) {
        for (int cx = 0; cx < 2; cx++) {
            int bx = cx ? x + w - r : x;
            int by = cy ? y + h - r : y;
            for (int py = by; py < by + r; py++) {
                for (int px = bx; px < bx + r; px++) {
                    int d8 = (int)pixel_dist8(px, py, cxs[cx], cys[cy]);
                    int dev = d8 - ring8; if (dev < 0) dev = -dev;
                    int cov = 256 - dev;
                    blend_cov(px, py, argb, a, cov);
                }
            }
        }
    }
    mark(x, y, w, h);
}

void fb_fill_circle(int cx, int cy, int r, uint32_t argb) {
    if (!g_ready || r <= 0) return;
    uint32_t a = (argb >> 24) ? (argb >> 24) : 255;
    uint32_t fill = (a << 24) | (argb & 0x00FFFFFF);
    for (int py = cy - r; py <= cy + r; py++) {
        for (int px = cx - r; px <= cx + r; px++) {
            uint32_t d8 = pixel_dist8(px, py, cx, cy);
            int cov = arc_coverage(d8, r);
            if (cov >= 256 && a == 255) bp(px, py, fill);
            else blend_cov(px, py, argb, a, cov);
        }
    }
    mark(cx - r, cy - r, 2*r + 1, 2*r + 1);
}

void fb_shadow_rounded_rect(int x, int y, int w, int h, int r,
                            int blur, uint32_t argb) {
    if (!g_ready || w <= 0 || h <= 0 || blur <= 0) return;
    uint32_t base_a = (argb >> 24) & 0xFF;
    if (base_a == 0) return;
    uint32_t rgb = argb & 0x00FFFFFF;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    int x0 = x - blur, y0 = y - blur;
    int x1 = x + w + blur, y1 = y + h + blur;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > g_w) x1 = g_w;
    if (y1 > g_h) y1 = g_h;

    /* Inner edges of the rounded-rect "core" the distance is measured to */
    int ix0 = (x + r) * 256, ix1 = (x + w - r) * 256;
    int iy0 = (y + r) * 256, iy1 = (y + h - r) * 256;
    int r8 = r * 256, b8 = blur * 256;

    for (int py = y0; py < y1; py++) {
        int pcy = py * 256 + 128;
        int qy = (pcy < iy0) ? iy0 - pcy : (pcy > iy1) ? pcy - iy1 : 0;
        for (int px = x0; px < x1; px++) {
            int pcx = px * 256 + 128;
            int qx = (pcx < ix0) ? ix0 - pcx : (pcx > ix1) ? pcx - ix1 : 0;

            int d8;
            if (qx == 0)      d8 = qy - r8;
            else if (qy == 0) d8 = qx - r8;
            else              d8 = (int)isqrt32((uint32_t)(qx >> 4) * (qx >> 4)
                                              + (uint32_t)(qy >> 4) * (qy >> 4)) * 16 - r8;
            if (d8 <= 0 || d8 >= b8) continue;     /* under the rect / out of range */

            /* Quadratic falloff approximates a gaussian penumbra */
            uint32_t t = 256 - (uint32_t)d8 * 256 / (uint32_t)b8;   /* 0..256 */
            uint32_t fall = (t * t) >> 8;
            uint32_t sa = (base_a * fall) >> 8;
            if (sa) bp_blend(px, py, (sa << 24) | rgb);
        }
    }
    mark(x - blur, y - blur, w + 2*blur, h + 2*blur);
}

/* ─── Instrument Sans text engine ────────────────────────────────────── */
static const ifont_face_t* font_face(int font) {
    if (font < 0 || font >= IFONT_COUNT) font = IFONT_BODY;
    return &g_ifonts[font];
}

void fb_draw_text(const char* s, int x, int y, int font, uint32_t argb) {
    if (!g_ready || !s) return;
    const ifont_face_t* f = font_face(font);
    uint32_t a = (argb >> 24) ? (argb >> 24) : 255;
    uint32_t rgb = argb & 0x00FFFFFF;
    int pen = x, top = y;
    int min_x = x, max_x = x, max_y = y;

    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\n') { pen = x; top += f->line_h; continue; }
        if (c < IFONT_FIRST || c > IFONT_LAST) c = '?';
        const ifont_glyph_t* g = &f->glyphs[c - IFONT_FIRST];
        const uint8_t* cov = f->bitmap + g->off;
        int gx = pen + g->bx, gy = top + g->by;
        for (int ry = 0; ry < g->h; ry++) {
            int py = gy + ry;
            if (py < 0 || py >= g_h) { cov += g->w; continue; }
            uint32_t* row = g_back + py * g_w;
            for (int rx = 0; rx < g->w; rx++) {
                uint8_t cv = cov[rx];
                if (!cv) continue;
                int px = gx + rx;
                if (px < 0 || px >= g_w) continue;
                uint32_t sa = ((uint32_t)cv * a) / 255;
                if (sa >= 255) row[px] = 0xFF000000u | rgb;
                else if (sa)   row[px] = fb_blend_pixel(row[px], (sa << 24) | rgb);
            }
            cov += g->w;
        }
        pen += g->adv;
        if (pen > max_x) max_x = pen;
        if (top + f->line_h > max_y) max_y = top + f->line_h;
    }
    mark(min_x - 2, y, max_x - min_x + 4, max_y - y);
}

int fb_text_width(const char* s, int font) {
    if (!s) return 0;
    const ifont_face_t* f = font_face(font);
    int line = 0, best = 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\n') { if (line > best) best = line; line = 0; continue; }
        if (c < IFONT_FIRST || c > IFONT_LAST) c = '?';
        line += f->glyphs[c - IFONT_FIRST].adv;
    }
    return line > best ? line : best;
}

int fb_font_height(int font) { return font_face(font)->line_h; }

/* ─── Legacy 8×16 bitmap text ────────────────────────────────────────── */
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
    mark(x, y, 8, 16);
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

void fb_draw_string_scaled(const char* s, uint32_t fg, int x, int y, int scale) {
    if (!g_ready || !s || scale < 1) return;
    uint32_t fgc = fg | 0xFF000000u;
    int cx = x;
    while (*s) {
        if (*s == '\n') { y += 16 * scale; cx = x; s++; continue; }
        const uint8_t* glyph = g_font_8x16[(unsigned char)*s];
        for (int row = 0; row < 16; row++) {
            uint8_t bits = glyph[row];
            if (!bits) continue;
            for (int col = 0; col < 8; col++) {
                if (!(bits & (0x80 >> col))) continue;
                fb_fill_rect(cx + col * scale, y + row * scale, scale, scale, fgc);
            }
        }
        cx += 8 * scale; s++;
    }
}

/* ─── Wallpaper ──────────────────────────────────────────────────────── */
/* Blob emitted by tools/generate_wallpaper.py and linked into the kernel:
 * two little-endian uint32 (width, height) followed by w*h ARGB pixels.
 * width == 0 means no wallpaper was embedded. */
extern const uint32_t g_wallpaper_blob[];

static uint32_t* g_wp_cache   = 0;   /* wallpaper pre-scaled to screen size */
static int       g_wp_cache_ok = 0;

int fb_draw_wallpaper(void) {
    if (!g_ready) return 0;
    int ww = (int)g_wallpaper_blob[0];
    int wh = (int)g_wallpaper_blob[1];
    if (ww <= 0 || wh <= 0) return 0;

    if (!g_wp_cache_ok) {
        g_wp_cache = (uint32_t*)kmalloc((size_t)g_w * g_h * 4);
        if (!g_wp_cache) return 0;
        const uint32_t* src = g_wallpaper_blob + 2;
        /* Cover-fit: scale up so both axes are filled, crop the overflow,
         * keeping the crop centred. 16.16 fixed point, bilinear sampling. */
        uint32_t sx_step, sy_step;
        int crop_x = 0, crop_y = 0;
        /* All products stay below 2^31 (source/screen dims are a few
         * thousand), so plain 32-bit math avoids libgcc 64-bit division */
        if (ww * g_h >= g_w * wh) {
            /* source is wider: fit height, crop left/right */
            sy_step = ((uint32_t)wh << 16) / g_h;
            sx_step = sy_step;
            crop_x = (ww - g_w * wh / g_h) / 2;
        } else {
            /* source is taller: fit width, crop top/bottom */
            sx_step = ((uint32_t)ww << 16) / g_w;
            sy_step = sx_step;
            crop_y = (wh - g_h * ww / g_w) / 2;
        }
        for (int y = 0; y < g_h; y++) {
            uint32_t fy = ((uint32_t)crop_y << 16) + (uint32_t)y * sy_step;
            uint32_t* drow = g_wp_cache + (size_t)y * g_w;
            uint32_t fx = (uint32_t)crop_x << 16;
            for (int x = 0; x < g_w; x++, fx += sx_step) {
                drow[x] = bilinear(src, ww, wh, fx, fy) | 0xFF000000u;
            }
        }
        g_wp_cache_ok = 1;
    }

    memcpy(g_back, g_wp_cache, (size_t)g_w * g_h * 4);
    mark(0, 0, g_w, g_h);
    return 1;
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
    mark(0, 0, g_w, g_h);
}

/* ─── Cursor overlay ─────────────────────────────────────────────────── */
#define CURSOR_MAX 32
static uint32_t g_cur_img[CURSOR_MAX * CURSOR_MAX];
static int g_cur_w = 0, g_cur_h = 0;        /* 0 = no cursor          */
static int g_cur_x = 0, g_cur_y = 0;        /* desired position       */
static int g_cur_drawn_x = -1, g_cur_drawn_y = -1;
static int g_cur_drawn = 0;                 /* stamped in VRAM now?   */
static fbrect_t g_page_cur[2];              /* cursor rect per page   */

void fb_set_cursor_image(const uint32_t* argb, int w, int h) {
    if (!argb || w <= 0 || h <= 0 || w > CURSOR_MAX || h > CURSOR_MAX) {
        g_cur_w = g_cur_h = 0;
        return;
    }
    for (int i = 0; i < w * h; i++) g_cur_img[i] = argb[i];
    g_cur_w = w; g_cur_h = h;
}

void fb_set_cursor_pos(int x, int y) {
    g_cur_x = x; g_cur_y = y;
}

/* Copy a back-buffer rectangle to the given VRAM page (clipped) */
static void front_copy(int page, int x, int y, int w, int h) {
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = x + w; if (x2 > g_w) x2 = g_w;
    int y2 = y + h; if (y2 > g_h) y2 = g_h;
    if (x1 >= x2 || y1 >= y2) return;
    size_t bytes = (size_t)(x2 - x1) * 4;
    uint8_t* base = (uint8_t*)g_fb + (uint32_t)page * (uint32_t)g_h * g_pitch;
    for (int py = y1; py < y2; py++) {
        uint32_t* src = g_back + (size_t)py * g_w + x1;
        uint32_t* dst = (uint32_t*)(base + (uint32_t)py * g_pitch) + x1;
        memcpy(dst, src, bytes);
    }
}

static void front_copy_rect(int page, const fbrect_t* r) {
    if (r->valid) front_copy(page, r->x0, r->y0, r->x1 - r->x0, r->y1 - r->y0);
}

/* Alpha-stamp the cursor into the given VRAM page over the scene */
static void cursor_stamp(int page) {
    if (g_cur_w <= 0) { g_cur_drawn = 0; g_page_cur[page].valid = 0; return; }
    uint8_t* base = (uint8_t*)g_fb + (uint32_t)page * (uint32_t)g_h * g_pitch;
    for (int sy = 0; sy < g_cur_h; sy++) {
        int py = g_cur_y + sy;
        if (py < 0 || py >= g_h) continue;
        uint32_t* brow = g_back + (size_t)py * g_w;
        uint32_t* frow = (uint32_t*)(base + (uint32_t)py * g_pitch);
        for (int sx = 0; sx < g_cur_w; sx++) {
            int px = g_cur_x + sx;
            if (px < 0 || px >= g_w) continue;
            uint32_t p = g_cur_img[sy * g_cur_w + sx];
            uint8_t pa = (p >> 24) & 0xFF;
            if (pa == 0) continue;
            frow[px] = (pa == 255) ? p : fb_blend_pixel(brow[px], p);
        }
    }
    g_cur_drawn_x = g_cur_x;
    g_cur_drawn_y = g_cur_y;
    g_cur_drawn = 1;
    g_page_cur[page] = (fbrect_t){ g_cur_x, g_cur_y,
                                   g_cur_x + g_cur_w, g_cur_y + g_cur_h, 1 };
}

void fb_flush(void) {
    if (!g_ready || !g_back || !g_fb) return;

    int cursor_moved = g_cur_drawn &&
        (g_cur_drawn_x != g_cur_x || g_cur_drawn_y != g_cur_y);

    if (g_flip) {
        /* Tear-free path: bring the hidden page up to date with the back
         * buffer, stamp the cursor on it, then flip the scanout to it. */
        if (!g_dirty && !cursor_moved && g_cur_drawn) return;

        int t = g_page ^ 1;
        front_copy_rect(t, &g_stale[t]);      /* changes it missed       */
        front_copy_rect(t, &g_page_cur[t]);   /* erase its old cursor    */
        if (g_dirty)
            front_copy(t, g_dx0, g_dy0, g_dx1 - g_dx0, g_dy1 - g_dy0);
        cursor_stamp(t);

        g_stale[t].valid = 0;
        if (g_dirty)   /* the page we're leaving misses this frame */
            rect_union(&g_stale[g_page], g_dx0, g_dy0, g_dx1, g_dy1);
        g_dirty = 0;

        fb_hw_flip(t * g_h);
        g_page = t;
        return;
    }

    /* Single-page path: erase the previously stamped cursor by re-copying
     * that rect from the (cursor-free) back buffer — unless the dirty
     * region is about to repaint it anyway. */
    if (g_cur_drawn && (cursor_moved || g_dirty))
        front_copy(0, g_cur_drawn_x, g_cur_drawn_y, g_cur_w, g_cur_h);

    if (g_dirty) {
        front_copy(0, g_dx0, g_dy0, g_dx1 - g_dx0, g_dy1 - g_dy0);
        g_dirty = 0;
    }

    cursor_stamp(0);
}

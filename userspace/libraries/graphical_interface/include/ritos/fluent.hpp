#ifndef RITOS_FLUENT_HPP
#define RITOS_FLUENT_HPP

/*
 * Fluent — RitOS's Windows-11-style design tokens and procedural glyphs.
 * Header-only so every module (shell components and apps alike) can link
 * it without an extra object; everything draws through rit::System.
 */

#include <rit/system.hpp>
#include <stdint.h>

namespace fluent {

/* ── Design tokens (Windows 11 light theme) ───────────────────────────── */
constexpr uint32_t MICA        = 0xFFF3F3F3u;  /* window / titlebar surface  */
constexpr uint32_t CARD        = 0xFFFFFFFFu;  /* content surface            */
constexpr uint32_t CARD_ALT    = 0xFFFAFAFAu;  /* secondary surface          */
constexpr uint32_t STROKE      = 0xFFE5E5E5u;  /* card stroke                */
constexpr uint32_t BORDER      = 0xFFC9C9C9u;  /* window border              */
constexpr uint32_t BORDER_DIM  = 0xFFDDDDDDu;
constexpr uint32_t TEXT        = 0xFF1B1B1Bu;
constexpr uint32_t TEXT_SEC    = 0xFF5D5D5Du;
constexpr uint32_t TEXT_DIS    = 0xFFA0A0A0u;
constexpr uint32_t TEXT_WHITE  = 0xFFFFFFFFu;
constexpr uint32_t ACCENT      = 0xFF0067C0u;  /* Win11 blue                 */
constexpr uint32_t ACCENT_HOV  = 0xFF1975C5u;
constexpr uint32_t ACCENT_SOFT = 0xFFCCE4F7u;  /* selection fill             */
constexpr uint32_t HOVER       = 0xFFEAEAEAu;
constexpr uint32_t PRESSED     = 0xFFE0E0E0u;
constexpr uint32_t CLOSE_RED   = 0xFFC42B1Cu;
constexpr uint32_t FOLDER_DARK = 0xFFE8A33Du;  /* folder back tab            */
constexpr uint32_t FOLDER_MAIN = 0xFFFFCE4Au;  /* folder front               */
constexpr uint32_t DANGER      = 0xFFD13438u;

/* Taskbar / shell */
constexpr int      TASKBAR_H   = 48;
constexpr uint32_t TASKBAR_BG  = 0xF0F3F3F3u;  /* acrylic-ish over wallpaper */
constexpr uint32_t MENU_BG     = 0xFAF9F9F9u;

/* Window chrome */
constexpr int TITLEBAR_PX = 32;   /* titlebar height in pixels  */
constexpr int CAPTION_W   = 46;   /* caption button width       */

/* System font faces (Instrument Sans, antialiased) */
constexpr int FONT_SMALL = RITOS_FONT_SMALL;
constexpr int FONT_BODY  = RITOS_FONT_BODY;
constexpr int FONT_BOLD  = RITOS_FONT_BODY_BOLD;
constexpr int FONT_TITLE = RITOS_FONT_TITLE;
constexpr int FONT_H2    = RITOS_FONT_H2;
constexpr int FONT_H1    = RITOS_FONT_H1;

inline int text_w(const char* s, int font = FONT_BODY)
    { return rit::System::fb_text_width(s, font); }
inline int font_h(int font = FONT_BODY)
    { return rit::System::fb_font_height(font); }

/* ── Basic shapes ─────────────────────────────────────────────────────── */
inline void rect(int x, int y, int w, int h, uint32_t c)
    { rit::System::fb_fill_rect(x, y, w, h, c); }
inline void blend(int x, int y, int w, int h, uint32_t c)
    { rit::System::fb_fill_rect_blend(x, y, w, h, c); }
inline void rrect(int x, int y, int w, int h, int r, uint32_t c)
    { rit::System::fb_fill_rounded_rect(x, y, w, h, r, c); }
inline void stroke(int x, int y, int w, int h, int r, uint32_t c)
    { rit::System::fb_stroke_rounded_rect(x, y, w, h, r, c); }
inline void shadow(int x, int y, int w, int h, int r, int blur, uint32_t c)
    { rit::System::fb_shadow_rounded_rect(x, y, w, h, r, blur, c); }
inline void text(const char* s, uint32_t fg, int x, int y, int font = FONT_BODY)
    { rit::System::fb_draw_text(s, x, y, font, fg); }
inline void text2x(const char* s, uint32_t fg, int x, int y)
    { rit::System::fb_draw_text(s, x, y, FONT_H2, fg); }
/* Centre a string horizontally inside [x, x+w) */
inline void text_centered(const char* s, uint32_t fg, int x, int w, int y,
                          int font = FONT_BODY)
    { rit::System::fb_draw_text(s, x + (w - text_w(s, font)) / 2, y, font, fg); }

/* 1px-stroke rounded rectangle: AA fill + AA outline */
inline void rrect_border(int x, int y, int w, int h, int r,
                         uint32_t stroke_c, uint32_t fill) {
    rrect(x, y, w, h, r, fill);
    stroke(x, y, w, h, r, stroke_c);
}

/* Win11-style button (rounded 4px, subtle stroke) */
inline void button(int x, int y, int w, int h, const char* label,
                   bool hovered, bool accent = false) {
    uint32_t bg = accent ? (hovered ? ACCENT_HOV : ACCENT)
                         : (hovered ? HOVER : CARD_ALT);
    uint32_t fg = accent ? TEXT_WHITE : TEXT;
    rrect(x, y, w, h, 4, bg);
    if (!accent) stroke(x, y, w, h, 4, 0x30000000u);
    text(label, fg, x + (w - text_w(label, FONT_TITLE)) / 2,
         y + (h - font_h(FONT_TITLE)) / 2, FONT_TITLE);
}

/* ── Procedural glyphs (all drawn into a size*size box at x,y) ────────── */

/* Windows logo: four blue rounded panes */
inline void glyph_logo(int x, int y, int size, uint32_t c = 0xFF2E8FE8u) {
    int g = size / 10; if (g < 1) g = 1;          /* gap   */
    int p = (size - g) / 2;                       /* pane  */
    rect(x,         y,         p, p, c);
    rect(x + p + g, y,         p, p, c);
    rect(x,         y + p + g, p, p, c);
    rect(x + p + g, y + p + g, p, p, c);
}

/* Folder icon (Win11 yellow), size ~ width in px */
inline void glyph_folder(int x, int y, int size) {
    int h = size * 3 / 4;
    int tab_w = size * 2 / 5;
    rrect(x, y + 1, tab_w, h / 2, 2, FOLDER_DARK);            /* back tab  */
    rrect(x, y + size / 5, size, h, 2, FOLDER_DARK);          /* back body */
    rrect(x, y + size / 5 + 1, size, h - 1, 2, FOLDER_MAIN);  /* front     */
}

/* Document icon: white page, grey stroke, accent text lines */
inline void glyph_doc(int x, int y, int size, uint32_t line_col = 0xFF9DB8D9u) {
    int w = size * 3 / 4, h = size;
    rrect_border(x + (size - w) / 2, y, w, h, 2, 0xFFB5B5B5u, CARD);
    int lx = x + (size - w) / 2 + 3;
    for (int i = 0; i < 3; i++)
        rect(lx, y + 4 + i * (h / 4), w - 6, 2, line_col);
}

/* Executable/app icon fallback: accent rounded square with spark */
inline void glyph_app(int x, int y, int size) {
    rrect(x, y, size, size, 4, ACCENT);
    rect(x + size / 4, y + size / 2 - 1, size / 2, 2, TEXT_WHITE);
    rect(x + size / 2 - 1, y + size / 4, 2, size / 2, TEXT_WHITE);
}

/* Search magnifier. bg punches the lens hollow. */
inline void glyph_search(int x, int y, int size, uint32_t c = TEXT_SEC,
                         uint32_t bg = CARD) {
    int r = size * 2 / 5;
    rit::System::fb_fill_circle(x + r, y + r, r, c);
    rit::System::fb_fill_circle(x + r, y + r, r - 2, bg);
    for (int i = 0; i < size / 3 + 1; i++) {
        rect(x + r + r * 7 / 10 + i, y + r + r * 7 / 10 + i, 2, 2, c);
    }
}

/* Power: ring with a gap and a vertical dash on top. bg must match the
 * surface the glyph sits on (it punches the ring hollow). */
inline void glyph_power(int x, int y, int size, uint32_t c, uint32_t bg) {
    int r = size / 2 - 1;
    int cx = x + size / 2, cy = y + size / 2;
    rit::System::fb_fill_circle(cx, cy, r, c);
    rit::System::fb_fill_circle(cx, cy, r - 2, bg);
    rect(cx - 2, y - 1, 4, size / 2, bg);     /* gap on top */
    rect(cx - 1, y + 1, 2, size / 2 - 1, c);  /* the dash   */
}

/* Restart: broken ring + small arrow head. bg as in glyph_power. */
inline void glyph_restart(int x, int y, int size, uint32_t c, uint32_t bg) {
    int r = size / 2 - 1;
    int cx = x + size / 2, cy = y + size / 2;
    rit::System::fb_fill_circle(cx, cy, r, c);
    rit::System::fb_fill_circle(cx, cy, r - 2, bg);
    rect(cx, y - 1, r + 2, size / 3, bg);     /* gap (top-right) */
    /* arrow head pointing down at the gap end */
    for (int i = 0; i < 4; i++)
        rect(cx + r - 1 - i, y + size / 3 - i, 1 + 2 * i, 1, c);
}

/* Chevrons */
inline void chevron_right(int x, int y, int size, uint32_t c = TEXT_SEC) {
    int half = size / 2;
    for (int i = 0; i < half; i++) {
        rect(x + i, y + i, 2, 2, c);
        rect(x + i, y + size - 2 - i, 2, 2, c);
    }
}
inline void chevron_left(int x, int y, int size, uint32_t c = TEXT_SEC) {
    int half = size / 2;
    for (int i = 0; i < half; i++) {
        rect(x + half - 1 - i, y + i, 2, 2, c);
        rect(x + half - 1 - i, y + size - 2 - i, 2, 2, c);
    }
}
inline void arrow_up(int x, int y, int size, uint32_t c = TEXT_SEC) {
    int half = size / 2;
    for (int i = 0; i < half; i++) {
        rect(x + half - 1 - i, y + 2 + i, 2, 2, c);
        rect(x + half - 1 + i, y + 2 + i, 2, 2, c);
    }
    rect(x + half - 1, y + 2, 2, size - 4, c);
}

/* Close cross, centred in a box: 1px diagonals with soft AA shoulders */
inline void glyph_cross(int cx, int cy, int arm, uint32_t c) {
    uint32_t soft = (c & 0x00FFFFFF) | 0x58000000u;
    for (int d = -arm; d <= arm; d++) {
        rect(cx + d, cy + d, 1, 1, c);
        rect(cx + d, cy - d, 1, 1, c);
        blend(cx + d + 1, cy + d, 1, 1, soft);
        blend(cx + d - 1, cy + d, 1, 1, soft);
        blend(cx + d + 1, cy - d, 1, 1, soft);
        blend(cx + d - 1, cy - d, 1, 1, soft);
    }
}

/* Trash can */
inline void glyph_trash(int x, int y, int size, uint32_t c = TEXT_SEC) {
    rect(x, y + 2, size, 2, c);                     /* lid       */
    rect(x + size / 3, y, size / 3, 2, c);          /* handle    */
    rrect_border(x + 1, y + 5, size - 2, size - 5, 2, c, CARD);
    rect(x + size / 2 - 2, y + 7, 1, size - 9, c);  /* ribs      */
    rect(x + size / 2 + 1, y + 7, 1, size - 9, c);
}

/* Pen (rename) */
inline void glyph_pen(int x, int y, int size, uint32_t c = TEXT_SEC) {
    for (int i = 0; i < size - 4; i++)
        rect(x + size - 4 - i, y + 2 + i, 3, 2, c);
    rect(x, y + size - 3, 4, 3, c);                 /* tip */
}

/* Copy: two offset squares */
inline void glyph_copy(int x, int y, int size, uint32_t c = TEXT_SEC) {
    int s = size * 2 / 3;
    rrect_border(x + size - s, y, s, s, 2, c, CARD);
    rrect_border(x, y + size - s, s, s, 2, c, CARD);
}

/* Scissors-lite (cut): X with handles */
inline void glyph_cut(int x, int y, int size, uint32_t c = TEXT_SEC) {
    glyph_cross(x + size / 2, y + size / 2 - 1, size / 2 - 2, c);
    rect(x, y + size - 3, 3, 3, c);
    rect(x + size - 3, y + size - 3, 3, 3, c);
}

/* Clipboard (paste) */
inline void glyph_paste(int x, int y, int size, uint32_t c = TEXT_SEC) {
    rrect_border(x + 1, y + 1, size - 2, size - 1, 2, c, CARD);
    rect(x + size / 3, y, size / 3 + 1, 3, c);
    rect(x + 4, y + 5, size - 8, 1, c);
    rect(x + 4, y + 8, size - 8, 1, c);
}

/* Plus */
inline void glyph_plus(int x, int y, int size, uint32_t c = TEXT_SEC) {
    rect(x, y + size / 2 - 1, size, 2, c);
    rect(x + size / 2 - 1, y, 2, size, c);
}

/* Refresh-ish circular arrow reuses restart */
inline void glyph_refresh(int x, int y, int size, uint32_t c, uint32_t bg) {
    glyph_restart(x, y, size, c, bg);
}

/* Home: roof + body */
inline void glyph_home(int x, int y, int size, uint32_t c = TEXT_SEC) {
    int half = size / 2;
    for (int i = 0; i < half; i++) {
        rect(x + half - 1 - i, y + i, 2, 1, c);
        rect(x + half - 1 + i, y + i, 2, 1, c);
    }
    rrect_border(x + 2, y + half, size - 4, size - half, 1, c, CARD);
}

/* Monitor (This PC) */
inline void glyph_monitor(int x, int y, int size, uint32_t c = TEXT_SEC) {
    rrect_border(x, y + 1, size, size * 2 / 3, 2, c, CARD);
    rect(x + size / 2 - 1, y + size * 2 / 3 + 1, 2, 3, c);
    rect(x + size / 4, y + size - 2, size / 2, 2, c);
}

/* Gear-lite (settings): ring with 4 teeth */
inline void glyph_gear(int x, int y, int size, uint32_t c = TEXT_SEC) {
    int cx = x + size / 2, cy = y + size / 2, r = size / 2 - 2;
    rect(cx - 1, y, 3, 3, c);  rect(cx - 1, y + size - 3, 3, 3, c);
    rect(x, cy - 1, 3, 3, c);  rect(x + size - 3, cy - 1, 3, 3, c);
    rit::System::fb_fill_circle(cx, cy, r, c);
    rit::System::fb_fill_circle(cx, cy, r - 2, CARD);
}

} // namespace fluent

#endif

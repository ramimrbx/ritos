#include "../include/ritos/window.hpp"
#include "../../framework/include/rit/system.hpp"
#include "../../../kernel/include/kernel/palette.h"

/* Color constants */
#define C_WIN_TITLE1   0xFF1C2E54u  /* navy blue titlebar base */
#define C_WIN_TITLE2   0xFF142240u  /* darker bottom */
#define C_WIN_TITLE_A  0xFF243A70u  /* active titlebar highlight */
#define C_WIN_TITLE_I  0xFF10192Eu  /* inactive titlebar */
#define C_WIN_BORDER   0xFF2A3A60u
#define C_WIN_BORDER_A 0xFF4A80D0u  /* vivid blue active border */
#define C_SHADOW       0x70000000u
#define C_CLOSE_BTN    0xFFFF5F57u  /* macOS red */
#define C_MIN_BTN      0xFFFEBC2Eu  /* macOS yellow */
#define C_MAX_BTN      0xFF28C840u  /* macOS green */
#define C_BTN_OFF      0xFF3A4258u  /* inactive buttons */
#define C_TEXT_LIGHT   0xFFECF0FFu
#define C_TEXT_DIM     0xFF7A96B8u

/* Same palette the kernel uses for char-grid rendering, so the window body
 * matches the background the apps draw their text onto. */
static const uint32_t k_palette[16] = RITOS_PALETTE_INIT;

namespace ritos {

Window::Window(const char* title, int x, int y, int w, int h)
    : m_title(title), m_x(x), m_y(y), m_width(w), m_height(h), m_active(true),
      m_title_color(rit::Color::White), m_border_color(rit::Color::LightCyan),
      m_body_color(rit::Color::Black), m_content(""), m_visible(true),
      m_minimized(false), m_maximized(false),
      m_orig_x(x), m_orig_y(y), m_orig_width(w), m_orig_height(h) {}

Window::~Window() {}

void Window::set_content(const char* content) { m_content = content; }

void Window::set_active(bool active) {
    m_active = active;
    m_title_color  = active ? rit::Color::White    : rit::Color::LightGrey;
    m_border_color = active ? rit::Color::LightCyan : rit::Color::DarkGrey;
}

void Window::move(int dx, int dy) { m_x += dx; m_y += dy; }

/* ── Pixel rendering ─────────────────────────────────────────────────── */
void Window::draw() {
    if (!m_visible || m_minimized) return;

    if (!rit::System::fb_is_avail()) {
        /* ── Fallback: VGA text rendering ─── */
        char c_tl = m_active ? '\xC9' : '\xDA';
        char c_tr = m_active ? '\xBB' : '\xBF';
        char c_bl = m_active ? '\xC8' : '\xC0';
        char c_br = m_active ? '\xBC' : '\xD9';
        char c_h  = m_active ? '\xCD' : '\xC4';
        char c_v  = m_active ? '\xBA' : '\xB3';
        rit::Color border_c = m_active ? rit::Color::LightCyan : rit::Color::DarkGrey;
        rit::Color hdr_bg   = m_active ? rit::Color::Blue       : rit::Color::DarkGrey;
        rit::Color ttl_c    = m_active ? rit::Color::White       : rit::Color::LightGrey;

        rit::System::draw_char(c_tl, border_c, hdr_bg, m_x, m_y);
        for (int i = 1; i < m_width - 1; i++)
            rit::System::draw_char(c_h, border_c, hdr_bg, m_x + i, m_y);
        rit::System::draw_char(c_tr, border_c, hdr_bg, m_x + m_width - 1, m_y);

        int tlen = m_title.length();
        int tx   = m_x + (m_width - tlen) / 2;
        for (int i = 0; i < tlen; i++)
            rit::System::draw_char(m_title.c_str()[i], ttl_c, hdr_bg, tx + i, m_y);

        for (int row = 1; row < m_height - 1; row++) {
            rit::System::draw_char(c_v, border_c, m_body_color, m_x, m_y + row);
            for (int col = 1; col < m_width - 1; col++)
                rit::System::draw_char(' ', m_body_color, m_body_color, m_x + col, m_y + row);
            rit::System::draw_char(c_v, border_c, m_body_color, m_x + m_width - 1, m_y + row);
        }
        rit::System::draw_char(c_bl, border_c, m_body_color, m_x, m_y + m_height - 1);
        for (int i = 1; i < m_width - 1; i++)
            rit::System::draw_char(c_h, border_c, m_body_color, m_x + i, m_y + m_height - 1);
        rit::System::draw_char(c_br, border_c, m_body_color, m_x + m_width - 1, m_y + m_height - 1);
        return;
    }

    /* ── Pixel path ─── */
    int px = m_x * 8;
    int py = m_y * 16;
    int pw = m_width  * 8;
    int ph = m_height * 16;

    /* Soft drop shadow (two layers) */
    rit::System::fb_fill_rect_blend(px + 6, py + 8, pw, ph, 0x38000000u);
    rit::System::fb_fill_rect_blend(px + 3, py + 4, pw, ph, 0x48000000u);

    /* Outer border */
    uint32_t border_col = m_active ? C_WIN_BORDER_A : C_WIN_BORDER;
    rit::System::fb_fill_rounded_rect(px - 1, py - 1, pw + 2, ph + 2, 6, border_col);

    /* Title bar (24px) with gradient */
    static const int TB = 24; /* titlebar height in pixels */
    uint32_t t1 = m_active ? C_WIN_TITLE_A : C_WIN_TITLE1;
    uint32_t t2 = m_active ? C_WIN_TITLE2  : C_WIN_TITLE_I;
    rit::System::fb_fill_grad_v(px, py, pw, TB, t1, t2);
    /* subtle top highlight */
    rit::System::fb_fill_rect_blend(px, py, pw, 1, 0x50FFFFFFu);

    /* Window body matches the char-grid colour the app draws onto */
    uint32_t body_col = k_palette[(int)m_body_color & 0xF];
    rit::System::fb_fill_rect(px, py + TB, pw, ph - TB, body_col);

    /* Separator line below titlebar */
    rit::System::fb_draw_hline_px(px, py + TB - 1, pw, border_col);

    /* Control buttons — traffic-light circles with glyphs.
     * Geometry must stay in sync with the hit test in the desktop app. */
    int btn_y  = py + TB / 2;
    int btn_cl = px + pw - 16;  /* close    */
    int btn_mn = px + pw - 36;  /* minimize */
    int btn_mx = px + pw - 56;  /* maximize */
    int btn_r  = 6;

    rit::System::fb_fill_circle(btn_mx, btn_y, btn_r, m_active ? C_MAX_BTN   : C_BTN_OFF);
    rit::System::fb_fill_circle(btn_mn, btn_y, btn_r, m_active ? C_MIN_BTN   : C_BTN_OFF);
    rit::System::fb_fill_circle(btn_cl, btn_y, btn_r, m_active ? C_CLOSE_BTN : C_BTN_OFF);

    if (m_active) {
        uint32_t glyph = 0xFF40300Cu;
        /* close: x cross */
        for (int d = -2; d <= 2; d++) {
            rit::System::fb_fill_rect(btn_cl + d, btn_y + d, 1, 1, 0xFF7A1410u);
            rit::System::fb_fill_rect(btn_cl + d, btn_y - d, 1, 1, 0xFF7A1410u);
        }
        /* minimize: horizontal dash */
        rit::System::fb_fill_rect(btn_mn - 2, btn_y, 5, 1, glyph);
        /* maximize: small square outline */
        rit::System::fb_fill_rect(btn_mx - 2, btn_y - 2, 5, 1, 0xFF0A5A1Cu);
        rit::System::fb_fill_rect(btn_mx - 2, btn_y + 2, 5, 1, 0xFF0A5A1Cu);
        rit::System::fb_fill_rect(btn_mx - 2, btn_y - 2, 1, 5, 0xFF0A5A1Cu);
        rit::System::fb_fill_rect(btn_mx + 2, btn_y - 2, 1, 5, 0xFF0A5A1Cu);
    }

    /* Title text, left-aligned with a small accent dot */
    uint32_t ttl_col = m_active ? C_TEXT_LIGHT : C_TEXT_DIM;
    if (m_active)
        rit::System::fb_fill_circle(px + 11, py + TB / 2, 2, 0xFF5B8FFFu);
    rit::System::fb_draw_string_px(m_title.c_str(), ttl_col, 0, px + 20, py + (TB - 16) / 2, 1);

    /* Generic content text via compat char-grid path */
    const char* text  = m_content.c_str();
    int text_len = m_content.length();
    int max_w = m_width - 4;
    int max_h = m_height - 2;
    int ci = 0;
    for (int row = 0; row < max_h && ci < text_len; row++) {
        for (int col = 0; col < max_w && ci < text_len; col++) {
            char c = text[ci++];
            if (c == '\n') break;
            rit::System::draw_char(c, rit::Color::LightGrey,
                                   m_body_color,
                                   m_x + 2 + col, m_y + 1 + row);
        }
    }
}

} // namespace ritos

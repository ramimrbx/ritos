#include <ritos/window.hpp>
#include <ritos/fluent.hpp>
#include <rit/rbx_module.h>
#include <rit/system.hpp>
#include <kernel/palette.h>

/* Same palette the kernel uses for char-grid rendering, so legacy apps that
 * still draw text through the char grid land on a matching background. */
static const uint32_t k_palette[16] = RITOS_PALETTE_INIT;

/* Stored per module (each app statically links its own window.o); lets the
 * chrome show caption-button hover using the desktop's pixel mouse state. */
static const Desktop_Interface* g_win_di = 0;

namespace ritos {

void Window::set_desktop_interface(const Desktop_Interface* di) { g_win_di = di; }

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

    /* ── Pixel path: Windows 11 chrome ─── */
    int wx = m_x * 8;
    int wy = m_y * 16;
    int ww = m_width  * 8;
    int wh = m_height * 16;
    int radius = m_maximized ? 0 : 8;

    if (!m_maximized) {
        /* Soft drop shadow */
        fluent::blend(wx + 5, wy + 7, ww, wh, 0x26000000u);
        fluent::blend(wx + 2, wy + 3, ww, wh, 0x30000000u);
    }

    /* 1px border, then the window surface (titlebar = MICA) */
    uint32_t border_col = m_active ? 0xFFAAAAAAu : fluent::BORDER_DIM;
    fluent::rrect(wx - 1, wy - 1, ww + 2, wh + 2, radius ? radius + 1 : 0, border_col);
    fluent::rrect(wx, wy, ww, wh, radius, fluent::MICA);

    /* Content surface below the titlebar (square top edge) */
    uint32_t body_col = k_palette[(int)m_body_color & 0xF];
    if (m_body_color == rit::Color::Black || m_body_color == rit::Color::White)
        body_col = fluent::CARD;  /* default bodies become Win11 white */
    int body_r = radius ? radius - 1 : 0;
    fluent::rrect(wx, wy + kTitlebarPx, ww, wh - kTitlebarPx, body_r, body_col);
    fluent::rect(wx, wy + kTitlebarPx, ww, body_r + 1, body_col); /* square the top corners */
    rit::System::fb_draw_hline_px(wx, wy + kTitlebarPx - 1, ww, fluent::STROKE);

    /* Caption buttons: [minimize][maximize][close], 46x32 each, right edge */
    int cap_y  = wy;
    int x_cl   = wx + ww - fluent::CAPTION_W;          /* close    */
    int x_mx   = x_cl - fluent::CAPTION_W;             /* maximize */
    int x_mn   = x_mx - fluent::CAPTION_W;             /* minimize */

    int mxp = -1, myp = -1;
    if (g_win_di && g_win_di->get_mouse_px_x) {
        mxp = g_win_di->get_mouse_px_x();
        myp = g_win_di->get_mouse_px_y();
    }
    bool on_cap_row = (myp >= cap_y && myp < cap_y + kTitlebarPx);
    bool hov_cl = on_cap_row && mxp >= x_cl && mxp < x_cl + fluent::CAPTION_W;
    bool hov_mx = on_cap_row && mxp >= x_mx && mxp < x_mx + fluent::CAPTION_W;
    bool hov_mn = on_cap_row && mxp >= x_mn && mxp < x_mn + fluent::CAPTION_W;

    if (hov_mn) fluent::rect(x_mn, cap_y, fluent::CAPTION_W, kTitlebarPx, fluent::HOVER);
    if (hov_mx) fluent::rect(x_mx, cap_y, fluent::CAPTION_W, kTitlebarPx, fluent::HOVER);
    if (hov_cl) {
        fluent::rect(x_cl, cap_y, fluent::CAPTION_W - radius, kTitlebarPx, fluent::CLOSE_RED);
        fluent::rrect(x_cl, cap_y, fluent::CAPTION_W, kTitlebarPx, radius, fluent::CLOSE_RED);
        fluent::rect(x_cl, cap_y + kTitlebarPx - radius - 1, fluent::CAPTION_W, radius + 1, fluent::CLOSE_RED);
    }

    uint32_t glyph_c = m_active ? fluent::TEXT : fluent::TEXT_DIS;
    int gy = cap_y + kTitlebarPx / 2;

    /* minimize: 10px horizontal line */
    fluent::rect(x_mn + fluent::CAPTION_W / 2 - 5, gy, 10, 1, glyph_c);

    /* maximize: square outline (two offset squares when maximized = restore) */
    int sq_x = x_mx + fluent::CAPTION_W / 2 - 5;
    int sq_y = gy - 5;
    if (m_maximized) {
        /* back square (top-right, partial) */
        fluent::rect(sq_x + 3, sq_y - 2, 8, 1, glyph_c);
        fluent::rect(sq_x + 10, sq_y - 2, 1, 8, glyph_c);
        /* front square */
        fluent::rect(sq_x, sq_y, 8, 1, glyph_c);
        fluent::rect(sq_x, sq_y + 7, 8, 1, glyph_c);
        fluent::rect(sq_x, sq_y, 1, 8, glyph_c);
        fluent::rect(sq_x + 7, sq_y, 1, 8, glyph_c);
        /* knock out where front overlaps back */
        fluent::rect(sq_x + 1, sq_y + 1, 6, 6,
                     hov_mx ? fluent::HOVER : fluent::MICA);
    } else {
        fluent::rect(sq_x, sq_y, 10, 1, glyph_c);
        fluent::rect(sq_x, sq_y + 9, 10, 1, glyph_c);
        fluent::rect(sq_x, sq_y, 1, 10, glyph_c);
        fluent::rect(sq_x + 9, sq_y, 1, 10, glyph_c);
    }

    /* close: x cross (white on red hover) */
    fluent::glyph_cross(x_cl + fluent::CAPTION_W / 2 - 1, gy, 5,
                        hov_cl ? fluent::TEXT_WHITE : glyph_c);

    /* Title text, left-aligned */
    uint32_t ttl_col = m_active ? fluent::TEXT : fluent::TEXT_SEC;
    fluent::text(m_title.c_str(), ttl_col, wx + 16, wy + (kTitlebarPx - 16) / 2);

    /* Generic content text via compat char-grid path */
    const char* text  = m_content.c_str();
    int text_len = m_content.length();
    int max_w = m_width - 4;
    int max_h = m_height - 3;
    int ci = 0;
    for (int row = 0; row < max_h && ci < text_len; row++) {
        for (int col = 0; col < max_w && ci < text_len; col++) {
            char c = text[ci++];
            if (c == '\n') break;
            char tmp[2] = { c, 0 };
            rit::System::fb_draw_string_px(
                tmp, fluent::TEXT, 0,
                wx + 16 + col * 8, wy + kTitlebarPx + 8 + row * 16, 1);
        }
    }
}

} // namespace ritos

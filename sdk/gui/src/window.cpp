#include "../include/ritos/window.hpp"

namespace ritos {

Window::Window(const char* title, int x, int y, int w, int h)
	: m_title(title), m_x(x), m_y(y), m_width(w), m_height(h), m_active(true),
	  m_title_color(rit::Color::White), m_border_color(rit::Color::LightGrey),
	  m_body_color(rit::Color::Black), m_content(""), m_visible(true), m_minimized(false) {
}

Window::~Window() {}

void Window::set_content(const char* content) {
	m_content = content;
}

void Window::set_active(bool active) {
	m_active = active;
	m_title_color = active ? rit::Color::White : rit::Color::DarkGrey;
	m_border_color = active ? rit::Color::LightCyan : rit::Color::DarkGrey;
}

void Window::move(int dx, int dy) {
	m_x += dx;
	m_y += dy;
}

void Window::draw() {
	if (!m_visible || m_minimized) return;

	char c_tl = m_active ? '\xC9' : '\xDA'; // ╔ or ┌
	char c_tr = m_active ? '\xBB' : '\xBF'; // ╗ or ┐
	char c_bl = m_active ? '\xC8' : '\xC0'; // ╚ or └
	char c_br = m_active ? '\xBC' : '\xD9'; // ╝ or ┘
	char c_horiz = m_active ? '\xCD' : '\xC4'; // ═ or ─
	char c_vert = m_active ? '\xBA' : '\xB3'; // ║ or │

	rit::Color header_bg = m_active ? rit::Color::Blue : rit::Color::DarkGrey;

	// 1. Draw top border and title
	rit::System::draw_char(c_tl, m_border_color, header_bg, m_x, m_y);
	for (int i = 1; i < m_width - 1; i++) {
		rit::System::draw_char(c_horiz, m_border_color, header_bg, m_x + i, m_y);
	}
	rit::System::draw_char(c_tr, m_border_color, header_bg, m_x + m_width - 1, m_y);

	// Draw Title Centered
	int title_len = m_title.length();
	int title_x = m_x + (m_width - title_len) / 2;
	if (title_x > m_x && title_x < m_x + m_width - 1) {
		for (int i = 0; i < title_len; i++) {
			rit::System::draw_char(m_title.c_str()[i], m_title_color, header_bg, title_x + i, m_y);
		}
	}

	// Draw Close button [X] and Minimize button [-] on the header
	if (m_width >= 8) {
		// Draw Minimize [-]
		rit::System::draw_char('[', m_border_color, header_bg, m_x + m_width - 6, m_y);
		rit::System::draw_char('-', rit::Color::White, header_bg, m_x + m_width - 5, m_y);
		rit::System::draw_char(']', m_border_color, header_bg, m_x + m_width - 4, m_y);

		// Draw Close [X]
		rit::System::draw_char('[', m_border_color, header_bg, m_x + m_width - 3, m_y);
		rit::System::draw_char('X', rit::Color::LightRed, header_bg, m_x + m_width - 2, m_y);
		rit::System::draw_char(']', m_border_color, header_bg, m_x + m_width - 1, m_y);
	}

	// 2. Draw sides and background body
	for (int row = 1; row < m_height - 1; row++) {
		rit::System::draw_char(c_vert, m_border_color, m_body_color, m_x, m_y + row);
		for (int col = 1; col < m_width - 1; col++) {
			rit::System::draw_char(' ', m_body_color, m_body_color, m_x + col, m_y + row);
		}
		rit::System::draw_char(c_vert, m_border_color, m_body_color, m_x + m_width - 1, m_y + row);
	}

	// 3. Draw bottom border
	rit::System::draw_char(c_bl, m_border_color, m_body_color, m_x, m_y + m_height - 1);
	for (int i = 1; i < m_width - 1; i++) {
		rit::System::draw_char(c_horiz, m_border_color, m_body_color, m_x + i, m_y + m_height - 1);
	}
	rit::System::draw_char(c_br, m_border_color, m_body_color, m_x + m_width - 1, m_y + m_height - 1);

	// 4. Draw content (simplified single line wrapping for now)
	const char* text = m_content.c_str();
	int text_len = m_content.length();
	int max_content_w = m_width - 4; // Margin of 2 on each side
	int max_content_h = m_height - 2;

	int char_idx = 0;
	for (int row = 0; row < max_content_h && char_idx < text_len; row++) {
		for (int col = 0; col < max_content_w && char_idx < text_len; col++) {
			char c = text[char_idx++];
			if (c == '\n') {
				break; // Go to next row
			}
			rit::System::draw_char(c, rit::Color::White, m_body_color, m_x + 2 + col, m_y + 1 + row);
		}
	}
}

} // namespace ritos

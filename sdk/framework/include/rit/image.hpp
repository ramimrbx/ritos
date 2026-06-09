#ifndef RIT_IMAGE_HPP
#define RIT_IMAGE_HPP

#include "system.hpp"

namespace rit {

class Image {
private:
	int m_width;
	int m_height;
	const char* m_data;
	const uint8_t* m_colors;

public:
	Image(int w, int h, const char* data, const uint8_t* colors)
		: m_width(w), m_height(h), m_data(data), m_colors(colors) {}

	int get_width() const { return m_width; }
	int get_height() const { return m_height; }

	void draw(int start_x, int start_y) const {
		if (m_data == nullptr) return;
		for (int y = 0; y < m_height; y++) {
			for (int x = 0; x < m_width; x++) {
				int idx = y * m_width + x;
				char c = m_data[idx];
				uint8_t attr = m_colors ? m_colors[idx] : 0x0F;
				Color fg = (Color)(attr & 0x0F);
				Color bg = (Color)((attr & 0xF0) >> 4);
				System::draw_char(c, fg, bg, start_x + x, start_y + y);
			}
		}
	}
};

} // namespace rit

#endif

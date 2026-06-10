#ifndef MODERN_GUI_COMPOSITOR_HPP
#define MODERN_GUI_COMPOSITOR_HPP

#include <stdint.h>

namespace modern_gui {

struct Color {
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a;

	Color() : b(0), g(0), r(0), a(0) {}
	Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
		: b(blue), g(green), r(red), a(alpha) {}
	
	uint32_t to_u32() const {
		return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
	}

	static Color from_u32(uint32_t u) {
		return Color((u >> 16) & 0xFF, (u >> 8) & 0xFF, u & 0xFF, (u >> 24) & 0xFF);
	}
};

class Window {
private:
	int m_x, m_y;
	int m_width, m_height;
	uint32_t* m_offscreen_buffer; /* 32-bit ARGB local buffer */
	bool m_visible;
	int m_corner_radius;
	int m_shadow_offset_x;
	int m_shadow_offset_y;
	int m_shadow_blur;
	float m_shadow_opacity;

public:
	Window(int x, int y, int w, int h, int corner_radius = 12);
	~Window();

	int get_x() const { return m_x; }
	int get_y() const { return m_y; }
	int get_width() const { return m_width; }
	int get_height() const { return m_height; }
	bool is_visible() const { return m_visible; }
	int get_corner_radius() const { return m_corner_radius; }

	void set_position(int x, int y) { m_x = x; m_y = y; }
	void set_visible(bool visible) { m_visible = visible; }
	
	/* Draw window decoration controls (rounded corners, drop shadow) */
	void configure_shadow(int ox, int oy, int blur, float opacity);

	uint32_t* get_buffer() { return m_offscreen_buffer; }

	/* Anti-aliased rounded rectangle drawing in off-screen buffer */
	void draw_rounded_rect(int rx, int ry, int rw, int rh, int radius, Color color);
	void clear(Color color);
};

class Compositor {
private:
	uint32_t* m_screen_framebuffer; /* Points to the VBE linear framebuffer */
	uint32_t* m_back_buffer;        /* Screen-sized temporary buffer */
	int m_screen_width;
	int m_screen_height;
	
	Window* m_windows[32];
	int m_window_count;

	/* Blends a single pixel using alpha blending */
	static inline uint32_t blend_pixel(uint32_t dst, uint32_t src) {
		uint8_t sa = (src >> 24) & 0xFF;
		if (sa == 0) return dst;
		if (sa == 255) return src;

		uint8_t dr = (dst >> 16) & 0xFF;
		uint8_t dg = (dst >> 8) & 0xFF;
		uint8_t db = dst & 0xFF;

		uint8_t sr = (src >> 16) & 0xFF;
		uint8_t sg = (src >> 8) & 0xFF;
		uint8_t sb = src & 0xFF;

		uint32_t r = (sr * sa + dr * (255 - sa)) / 255;
		uint32_t g = (sg * sa + dg * (255 - sa)) / 255;
		uint32_t b = (sb * sa + db * (255 - sa)) / 255;

		return (255 << 24) | (r << 16) | (g << 8) | b;
	}

public:
	Compositor(uint32_t* framebuffer, int width, int height);
	~Compositor();

	void add_window(Window* win);
	void remove_window(Window* win);

	/* Composites all window buffers back-to-front with drop shadows and rounded corners */
	void composite_and_flush();

	/* Draw anti-aliased shadows on back buffer for a window */
	void draw_window_shadow(Window* win);
};

} // namespace modern_gui

#endif /* MODERN_GUI_COMPOSITOR_HPP */

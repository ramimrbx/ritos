#include "../include/modern_gui/compositor.hpp"

// Simple math functions to avoid depending on <cmath> when compiling freestanding
static inline float sqrt_approx(float number) {
	union {
		int i;
		float f;
	} conv;
	conv.f = number;
	conv.i = 0x5f3759df - (conv.i >> 1);
	float x2 = number * 0.5F;
	conv.f = conv.f * (1.5F - (x2 * conv.f * conv.f));
	return 1.0f / conv.f;
}

static inline float exp_approx(float x) {
	// Simple Taylor series approximation for e^x where x < 0
	if (x > 0.0f) return 1.0f;
	if (x < -8.0f) return 0.0f;
	float term = 1.0f;
	float sum = 1.0f;
	for (int i = 1; i <= 8; i++) {
		term *= x / (float)i;
		sum += term;
	}
	return sum;
}

namespace modern_gui {

Window::Window(int x, int y, int w, int h, int corner_radius)
	: m_x(x), m_y(y), m_width(w), m_height(h), m_visible(true), m_corner_radius(corner_radius),
	  m_shadow_offset_x(4), m_shadow_offset_y(4), m_shadow_blur(8), m_shadow_opacity(0.4f) {
	
	m_offscreen_buffer = new uint32_t[w * h];
	// Initialize with fully transparent black
	clear(Color(0, 0, 0, 0));
}

Window::~Window() {
	delete[] m_offscreen_buffer;
}

void Window::configure_shadow(int ox, int oy, int blur, float opacity) {
	m_shadow_offset_x = ox;
	m_shadow_offset_y = oy;
	m_shadow_blur = blur;
	m_shadow_opacity = opacity;
}

void Window::clear(Color color) {
	uint32_t val = color.to_u32();
	for (int i = 0; i < m_width * m_height; ++i) {
		m_offscreen_buffer[i] = val;
	}
}

void Window::draw_rounded_rect(int rx, int ry, int rw, int rh, int radius, Color color) {
	uint32_t color_val = color.to_u32();
	int x_end = rx + rw;
	int y_end = ry + rh;

	for (int y = ry; y < y_end; ++y) {
		if (y < 0 || y >= m_height) continue;
		for (int x = rx; x < x_end; ++x) {
			if (x < 0 || x >= m_width) continue;

			// Check if we are inside the corner quadrants
			bool top = (y < ry + radius);
			bool bottom = (y >= y_end - radius);
			bool left = (x < rx + radius);
			bool right = (x >= x_end - radius);

			float dist = 0.0f;
			bool is_corner = false;

			if (top && left) {
				float dx = (float)(x - (rx + radius));
				float dy = (float)(y - (ry + radius));
				dist = sqrt_approx(dx*dx + dy*dy);
				is_corner = true;
			} else if (top && right) {
				float dx = (float)(x - (x_end - 1 - radius));
				float dy = (float)(y - (ry + radius));
				dist = sqrt_approx(dx*dx + dy*dy);
				is_corner = true;
			} else if (bottom && left) {
				float dx = (float)(x - (rx + radius));
				float dy = (float)(y - (y_end - 1 - radius));
				dist = sqrt_approx(dx*dx + dy*dy);
				is_corner = true;
			} else if (bottom && right) {
				float dx = (float)(x - (x_end - 1 - radius));
				float dy = (float)(y - (y_end - 1 - radius));
				dist = sqrt_approx(dx*dx + dy*dy);
				is_corner = true;
			}

			if (is_corner) {
				if (dist > (float)radius + 0.5f) {
					// Outside corner circle: transparent
					continue;
				} else if (dist > (float)radius - 0.5f) {
					// Edge of corner: anti-alias blend
					float alpha_factor = ((float)radius + 0.5f - dist);
					if (alpha_factor < 0.0f) alpha_factor = 0.0f;
					if (alpha_factor > 1.0f) alpha_factor = 1.0f;
					
					uint8_t blended_a = (uint8_t)((float)color.a * alpha_factor);
					Color edge_color(color.r, color.g, color.b, blended_a);
					
					// Dest blending inside the offscreen app buffer
					int idx = y * m_width + x;
					m_offscreen_buffer[idx] = Compositor::blend_pixel(m_offscreen_buffer[idx], edge_color.to_u32());
				} else {
					// Inside corner circle
					m_offscreen_buffer[y * m_width + x] = color_val;
				}
			} else {
				// Inside center body
				m_offscreen_buffer[y * m_width + x] = color_val;
			}
		}
	}
}


Compositor::Compositor(uint32_t* framebuffer, int width, int height)
	: m_screen_framebuffer(framebuffer), m_screen_width(width), m_screen_height(height), m_window_count(0) {
	m_back_buffer = new uint32_t[width * height];
	for (int i = 0; i < 32; ++i) m_windows[i] = nullptr;
}

Compositor::~Compositor() {
	delete[] m_back_buffer;
}

void Compositor::add_window(Window* win) {
	if (m_window_count < 32) {
		m_windows[m_window_count++] = win;
	}
}

void Compositor::remove_window(Window* win) {
	int idx = -1;
	for (int i = 0; i < m_window_count; ++i) {
		if (m_windows[i] == win) {
			idx = i;
			break;
		}
	}
	if (idx != -1) {
		for (int i = idx; i < m_window_count - 1; ++i) {
			m_windows[i] = m_windows[i + 1];
		}
		m_windows[--m_window_count] = nullptr;
	}
}

void Compositor::draw_window_shadow(Window* win) {
	// Shadow box parameters
	int sx = win->get_x() + win->m_shadow_offset_x;
	int sy = win->get_y() + win->m_shadow_offset_y;
	int sw = win->get_width();
	int sh = win->get_height();
	int blur = win->m_shadow_blur;
	float max_opacity = win->m_shadow_opacity;

	// Calculate shadow bounds including blur radius
	int min_x = sx - blur;
	int max_x = sx + sw + blur;
	int min_y = sy - blur;
	int max_y = sy + sh + blur;

	// Keep bounds within back buffer
	if (min_x < 0) min_x = 0;
	if (max_x > m_screen_width) max_x = m_screen_width;
	if (min_y < 0) min_y = 0;
	if (max_y > m_screen_height) max_y = m_screen_height;

	for (int y = min_y; y < max_y; ++y) {
		for (int x = min_x; x < max_x; ++x) {
			
			// Find distance to the window box boundary
			float dx = 0.0f;
			if (x < sx) dx = (float)(sx - x);
			else if (x >= sx + sw) dx = (float)(x - (sx + sw - 1));

			float dy = 0.0f;
			if (y < sy) dy = (float)(sy - y);
			else if (y >= sy + sh) dy = (float)(y - (sy + sh - 1));

			float dist = sqrt_approx(dx*dx + dy*dy);
			
			// Shadow falls off exponentially based on distance
			float factor = exp_approx(-dist / (float)blur);
			if (factor > 0.005f) {
				uint8_t shadow_a = (uint8_t)(max_opacity * factor * 255.0f);
				Color shadow_color(0, 0, 0, shadow_a);
				
				int idx = y * m_screen_width + x;
				m_back_buffer[idx] = blend_pixel(m_back_buffer[idx], shadow_color.to_u32());
			}
		}
	}
}

void Compositor::composite_and_flush() {
	// 1. Clear backbuffer with desktop background color (e.g. deep modern blue-grey)
	uint32_t bg_color = Color(30, 40, 50, 255).to_u32();
	for (int i = 0; i < m_screen_width * m_screen_height; ++i) {
		m_back_buffer[i] = bg_color;
	}

	// 2. Iterate back-to-front through visible windows
	for (int i = 0; i < m_window_count; ++i) {
		Window* win = m_windows[i];
		if (!win || !win->is_visible()) continue;

		// Draw window shadow
		draw_window_shadow(win);

		// Blit window offscreen buffer with alpha transparency support (rounded corners)
		int wx = win->get_x();
		int wy = win->get_y();
		int ww = win->get_width();
		int wh = win->get_height();
		uint32_t* win_buf = win->get_buffer();

		for (int y = 0; y < wh; ++y) {
			int screen_y = wy + y;
			if (screen_y < 0 || screen_y >= m_screen_height) continue;

			for (int x = 0; x < ww; ++x) {
				int screen_x = wx + x;
				if (screen_x < 0 || screen_x >= m_screen_width) continue;

				uint32_t src_pixel = win_buf[y * ww + x];
				int dest_idx = screen_y * m_screen_width + screen_x;
				
				m_back_buffer[dest_idx] = blend_pixel(m_back_buffer[dest_idx], src_pixel);
			}
		}
	}

	// 3. Flush final composited frame from back buffer to screen (VBE framebuffer)
	// Using simple copy loop for freestanding compatibility
	for (int i = 0; i < m_screen_width * m_screen_height; ++i) {
		m_screen_framebuffer[i] = m_back_buffer[i];
	}
}

} // namespace modern_gui

#ifndef RIT_SYSTEM_HPP
#define RIT_SYSTEM_HPP

#include "string.hpp"
#include "ritos_api.h"
#include <stdint.h>
#include <stddef.h>

namespace rit {

enum class Color : uint8_t {
	Black = 0,
	Blue = 1,
	Green = 2,
	Cyan = 3,
	Red = 4,
	Magenta = 5,
	Brown = 6,
	LightGrey = 7,
	DarkGrey = 8,
	LightBlue = 9,
	LightGreen = 10,
	LightCyan = 11,
	LightRed = 12,
	LightMagenta = 13,
	LightBrown = 14,
	White = 15,
};

class System {
public:
	static void print(const char* text);
	static void print(const String& text);
	static void println(const char* text);
	static void println(const String& text);
	static void clear_screen();
	static void set_color(Color fg, Color bg);
	static void draw_char(char c, Color fg, Color bg, int x, int y);
	
	// Mouse Driver API
	static void init_mouse();
	static bool poll_mouse(int& x, int& y, uint8_t& buttons);
	static bool poll_mouse_px(int& x, int& y, uint8_t& buttons);

	// Power Management API
	static void shutdown();
	static void reboot();

	// Keyboard Driver API
	static bool poll_keyboard(char& out_char);

	// System Information API
	static size_t get_heap_usage();
	static void get_time(int& hour, int& min, int& sec);
	static void get_date(int& year, int& month, int& day);

	// Double Buffering
	static void enable_double_buffer(bool enable);
	static void flush();

	// Framebuffer pixel API (no-ops when fb not available)
	static void fb_fill_rect(int x, int y, int w, int h, uint32_t argb);
	static void fb_fill_rect_blend(int x, int y, int w, int h, uint32_t argb);
	static void fb_blit_argb(const uint32_t* pixels, int x, int y, int w, int h);
	static void fb_draw_string_px(const char* text, uint32_t fg, uint32_t bg,
	                              int x, int y, int transparent_bg);
	static void fb_fill_grad_v(int x, int y, int w, int h,
	                           uint32_t top, uint32_t bot);
	static void fb_fill_grad_h(int x, int y, int w, int h,
	                           uint32_t left, uint32_t right);
	static void fb_fill_rounded_rect(int x, int y, int w, int h, int r,
	                                 uint32_t argb);
	static void fb_fill_circle(int cx, int cy, int r, uint32_t argb);
	static void fb_draw_hline_px(int x, int y, int w, uint32_t argb);
	static void fb_flush_px();
	static int  fb_get_width();
	static int  fb_get_height();
	static int  fb_is_avail();

	// Get the OS API function table
	static const RitOS_API* get_api_table();

	// .rbx dynamic binary loader
	static void* load_rbx(const char* filepath);

	// App Launching
	static void launch_app(const char* title);
	static void set_launch_app_handler(void (*handler)(const char* title));
};

} // namespace rit

#endif

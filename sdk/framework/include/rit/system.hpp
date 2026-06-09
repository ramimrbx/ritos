#ifndef RIT_SYSTEM_HPP
#define RIT_SYSTEM_HPP

#include "string.hpp"
#include <stdint.h>

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

	// Power Management API
	static void shutdown();
	static void reboot();

	// Keyboard Driver API
	static bool poll_keyboard(char& out_char);

	// System Information API
	static size_t get_heap_usage();
	static void get_time(int& hour, int& min, int& sec);
	static void get_date(int& year, int& month, int& day);
};

} // namespace rit

#endif

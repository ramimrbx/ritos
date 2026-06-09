#include "../include/rit/system.hpp"
#include "../../../kernel/include/kernel/terminal.h"
#include "../../../kernel/include/kernel/mouse.h"
#include "../../../kernel/include/kernel/power.h"
#include "../../../kernel/include/kernel/keyboard.h"
#include "../../../kernel/include/kernel/io.h"
#include "../../../kernel/include/kernel/memory.h"

namespace rit {

void System::print(const char* text) {
	Terminal_writestring(g_sys_terminal, text);
}

void System::print(const String& text) {
	Terminal_writestring(g_sys_terminal, text.c_str());
}

void System::println(const char* text) {
	Terminal_writestring(g_sys_terminal, text);
	Terminal_putchar(g_sys_terminal, '\n');
}

void System::println(const String& text) {
	Terminal_writestring(g_sys_terminal, text.c_str());
	Terminal_putchar(g_sys_terminal, '\n');
}

void System::clear_screen() {
	Terminal_clear(g_sys_terminal);
}

void System::set_color(Color fg, Color bg) {
	uint8_t color_byte = (uint8_t)fg | ((uint8_t)bg << 4);
	Terminal_set_color(g_sys_terminal, color_byte);
}

void System::draw_char(char c, Color fg, Color bg, int x, int y) {
	uint8_t color_byte = (uint8_t)fg | ((uint8_t)bg << 4);
	Terminal_putentryat(g_sys_terminal, c, color_byte, x, y);
}

void System::init_mouse() {
	mouse_init();
}

bool System::poll_mouse(int& x, int& y, uint8_t& buttons) {
	MouseState state;
	if (mouse_poll(&state)) {
		x = state.x;
		y = state.y;
		buttons = state.buttons;
		return true;
	}
	return false;
}

void System::shutdown() {
	sys_shutdown();
}

void System::reboot() {
	sys_reboot();
}

static uint8_t get_rtc_register(uint8_t reg) {
	outb(0x70, reg);
	return inb(0x71);
}

bool System::poll_keyboard(char& out_char) {
	return keyboard_poll(&out_char);
}

size_t System::get_heap_usage() {
	return ::get_heap_usage();
}

void System::get_time(int& hour, int& min, int& sec) {
	// Wait until RTC update is not in progress
	outb(0x70, 0x0A);
	while (inb(0x71) & 0x80) {
		// Wait
	}
	sec = get_rtc_register(0x00);
	min = get_rtc_register(0x02);
	hour = get_rtc_register(0x04);

	uint8_t registerB = get_rtc_register(0x0B);

	// Convert BCD to binary if necessary
	if (!(registerB & 4)) {
		sec = (sec & 0x0F) + ((sec / 16) * 10);
		min = (min & 0x0F) + ((min / 16) * 10);
		hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
	}

	// Convert 12-hour clock to 24-hour if necessary
	if (!(registerB & 2) && (hour & 0x80)) {
		hour = ((hour & 0x7F) + 12) % 24;
	}
}

void System::get_date(int& year, int& month, int& day) {
	// Wait until RTC update is not in progress
	outb(0x70, 0x0A);
	while (inb(0x71) & 0x80) {
		// Wait
	}
	day = get_rtc_register(0x07);
	month = get_rtc_register(0x08);
	year = get_rtc_register(0x09);

	uint8_t registerB = get_rtc_register(0x0B);

	// Convert BCD to binary if necessary
	if (!(registerB & 4)) {
		day = (day & 0x0F) + ((day / 16) * 10);
		month = (month & 0x0F) + ((month / 16) * 10);
		year = (year & 0x0F) + ((year / 16) * 10);
	}
}

} // namespace rit

#ifndef KERNEL_TERMINAL_H
#define KERNEL_TERMINAL_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct Terminal;

// VGA Color Constants
typedef enum {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
} VGA_Color;

// OOP Virtual Table for Terminal
typedef struct TerminalVtbl {
	void (*write)(struct Terminal* self, const char* data, size_t size);
	void (*putchar)(struct Terminal* self, char c);
	void (*clear)(struct Terminal* self);
	void (*set_color)(struct Terminal* self, uint8_t color);
	void (*putentryat)(struct Terminal* self, char c, uint8_t color, size_t x, size_t y);
} TerminalVtbl;

// OOP Class Struct for Terminal
typedef struct Terminal {
	const TerminalVtbl* vptr; // Pointer to virtual table
	size_t row;
	size_t column;
	uint8_t color;
	uint16_t* buffer;
} Terminal;

// Public Constructor & Methods (Interface wrappers)
void Terminal_init(Terminal* self);
void Terminal_write(Terminal* self, const char* data, size_t size);
void Terminal_writestring(Terminal* self, const char* data);
void Terminal_putchar(Terminal* self, char c);
void Terminal_clear(Terminal* self);
void Terminal_set_color(Terminal* self, uint8_t color);
void Terminal_putentryat(Terminal* self, char c, uint8_t color, size_t x, size_t y);

// Global OOP terminal instance
extern Terminal* g_sys_terminal;

#ifdef __cplusplus
}
#endif

#endif

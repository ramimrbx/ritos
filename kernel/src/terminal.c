#include "../include/kernel/terminal.h"
#include "../include/kernel/io.h"

static uint16_t s_back_buffer[80 * 25];
static int s_double_buffer_enabled = 0;

// Define the static virtual table for Terminal OOP implementation
static void terminal_impl_write(Terminal* self, const char* data, size_t size);
static void terminal_impl_putchar(Terminal* self, char c);
static void terminal_impl_clear(Terminal* self);
static void terminal_impl_set_color(Terminal* self, uint8_t color);
static void terminal_impl_putentryat(Terminal* self, char c, uint8_t color, size_t x, size_t y);

static const TerminalVtbl s_terminal_vtbl = {
	.write = terminal_impl_write,
	.putchar = terminal_impl_putchar,
	.clear = terminal_impl_clear,
	.set_color = terminal_impl_set_color,
	.putentryat = terminal_impl_putentryat
};

// Global system terminal instance
static Terminal s_system_terminal;
Terminal* g_sys_terminal = &s_system_terminal;

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
	return (uint16_t) uc | (uint16_t) color << 8;
}

void Terminal_init(Terminal* self) {
	self->vptr = &s_terminal_vtbl;
	self->row = 0;
	self->column = 0;
	self->color = (uint8_t)VGA_COLOR_LIGHT_GREY | ((uint8_t)VGA_COLOR_BLACK << 4);
	self->buffer = (uint16_t*) 0xB8000;
	self->vptr->clear(self);

	// Disable VGA hardware cursor to prevent flickering cursor
	outb(0x3D4, 0x0A);
	outb(0x3D5, 0x20);
}

static void terminal_impl_clear(Terminal* self) {
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			self->buffer[index] = vga_entry(' ', self->color);
		}
	}
	self->row = 0;
	self->column = 0;
}

static void terminal_impl_set_color(Terminal* self, uint8_t color) {
	self->color = color;
}

static void terminal_impl_putentryat(Terminal* self, char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	self->buffer[index] = vga_entry(c, color);
}

static void terminal_impl_scroll(Terminal* self) {
	// Shift rows up by 1
	for (size_t y = 1; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t target_idx = (y - 1) * VGA_WIDTH + x;
			const size_t source_idx = y * VGA_WIDTH + x;
			self->buffer[target_idx] = self->buffer[source_idx];
		}
	}
	// Clear the bottom row
	for (size_t x = 0; x < VGA_WIDTH; x++) {
		const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
		self->buffer[index] = vga_entry(' ', self->color);
	}
	self->row = VGA_HEIGHT - 1;
}

static void terminal_impl_putchar(Terminal* self, char c) {
	if (c == '\n') {
		self->column = 0;
		if (++self->row == VGA_HEIGHT) {
			terminal_impl_scroll(self);
		}
		return;
	}

	if (c == '\r') {
		self->column = 0;
		return;
	}

	self->vptr->putentryat(self, c, self->color, self->column, self->row);
	if (++self->column == VGA_WIDTH) {
		self->column = 0;
		if (++self->row == VGA_HEIGHT) {
			terminal_impl_scroll(self);
		}
	}
}

static void terminal_impl_write(Terminal* self, const char* data, size_t size) {
	for (size_t i = 0; i < size; i++) {
		self->vptr->putchar(self, data[i]);
	}
}

// OOP wrapper methods
void Terminal_write(Terminal* self, const char* data, size_t size) {
	self->vptr->write(self, data, size);
}

void Terminal_writestring(Terminal* self, const char* data) {
	size_t len = 0;
	while (data[len] != '\0') {
		len++;
	}
	self->vptr->write(self, data, len);
}

void Terminal_putchar(Terminal* self, char c) {
	self->vptr->putchar(self, c);
}

void Terminal_clear(Terminal* self) {
	self->vptr->clear(self);
}

void Terminal_set_color(Terminal* self, uint8_t color) {
	self->vptr->set_color(self, color);
}

void Terminal_putentryat(Terminal* self, char c, uint8_t color, size_t x, size_t y) {
	self->vptr->putentryat(self, c, color, x, y);
}

void Terminal_enable_double_buffer(Terminal* self, int enable) {
	s_double_buffer_enabled = enable;
	if (enable) {
		self->buffer = s_back_buffer;
		// Copy current VGA contents to back buffer
		uint16_t* src = (uint16_t*)0xB8000;
		for (int i = 0; i < 80 * 25; i++) {
			s_back_buffer[i] = src[i];
		}
	} else {
		self->buffer = (uint16_t*)0xB8000;
	}
}

void Terminal_flush(Terminal* self) {
	if (s_double_buffer_enabled) {
		uint16_t* dest = (uint16_t*)0xB8000;
		for (int i = 0; i < 80 * 25; i++) {
			if (dest[i] != s_back_buffer[i]) {
				dest[i] = s_back_buffer[i];
			}
		}
	}
}

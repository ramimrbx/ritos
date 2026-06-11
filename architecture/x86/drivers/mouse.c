#include <kernel/mouse.h>
#include <kernel/input_output.h>
#include <kernel/framebuffer.h>

static void mouse_wait(uint8_t type) {
	uint32_t timeout = 100000;
	if (type == 0) {
		while (timeout--) {
			if ((inb(0x64) & 1) == 1) return;
		}
	} else {
		while (timeout--) {
			if ((inb(0x64) & 2) == 0) return;
		}
	}
}

static void mouse_write(uint8_t val) {
	mouse_wait(1);
	outb(0x64, 0xD4);
	mouse_wait(1);
	outb(0x60, val);
}

static uint8_t mouse_read(void) {
	mouse_wait(0);
	return inb(0x60);
}

static int g_mouse_pixel_x = 512;
static int g_mouse_pixel_y = 384;
static uint8_t g_mouse_buttons = 0;

void mouse_init(void) {
	uint8_t status;

	/* Start centred on whatever screen we actually got */
	if (fb_is_available()) {
		g_mouse_pixel_x = fb_get_width()  / 2;
		g_mouse_pixel_y = fb_get_height() / 2;
	}

	// 1. Enable auxiliary mouse device
	mouse_wait(1);
	outb(0x64, 0xA8);

	// 2. Enable mouse interrupts in controller configuration
	mouse_wait(1);
	outb(0x64, 0x20);
	mouse_wait(0);
	status = inb(0x60) | 2; // Enable IRQ12 mouse interrupt
	
	mouse_wait(1);
	outb(0x64, 0x60);
	mouse_wait(1);
	outb(0x60, status);

	// 3. Set mouse default values
	mouse_write(0xF6);
	mouse_read(); // ACK

	// 4. Enable data reporting
	mouse_write(0xF4);
	mouse_read(); // ACK
}

static uint8_t mouse_cycle = 0;
static int8_t mouse_bytes[3];

bool mouse_poll(MouseState* state) {
	uint8_t status = inb(0x64);
	
	// Check if data is available and comes from auxiliary device (bit 0 & bit 5)
	if ((status & 1) == 0 || (status & 0x20) == 0) {
		return false;
	}

	uint8_t data = inb(0x60);

	// Alignment check: byte 0 of packet (mouse_cycle == 0) must have bit 3 set to 1
	if (mouse_cycle == 0) {
		if ((data & 8) == 0) {
			return false; // Discard out of sync byte
		}
	}

	mouse_bytes[mouse_cycle++] = (int8_t)data;

	if (mouse_cycle == 3) {
		mouse_cycle = 0;

		g_mouse_buttons = mouse_bytes[0] & 7;

		int x_delta = mouse_bytes[1];
		int y_delta = mouse_bytes[2];

		// Sign extension
		if (mouse_bytes[0] & 0x10) {
			x_delta |= ~0xFF;
		}
		if (mouse_bytes[0] & 0x20) {
			y_delta |= ~0xFF;
		}

		// Update coordinates in pixel space for smooth sub-character tracking
		// Standard mouse deltas are added directly
		g_mouse_pixel_x += x_delta;
		g_mouse_pixel_y -= y_delta;

		/* Clamp within the actual screen (VGA text fallback: 1024x768) */
		int max_x = fb_is_available() ? fb_get_width()  : 1024;
		int max_y = fb_is_available() ? fb_get_height() : 768;
		if (g_mouse_pixel_x < 0) g_mouse_pixel_x = 0;
		if (g_mouse_pixel_x >= max_x) g_mouse_pixel_x = max_x - 1;
		if (g_mouse_pixel_y < 0) g_mouse_pixel_y = 0;
		if (g_mouse_pixel_y >= max_y) g_mouse_pixel_y = max_y - 1;

		// Scale down to character cells (each cell is 8x16 pixels)
		state->x = g_mouse_pixel_x / 8;
		state->y = g_mouse_pixel_y / 16;
		state->pixel_x = g_mouse_pixel_x;
		state->pixel_y = g_mouse_pixel_y;
		state->buttons = g_mouse_buttons;
		return true;
	}

	return false;
}

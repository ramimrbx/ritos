#include "../include/kernel/keyboard.h"
#include "../include/kernel/io.h"

static const char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,   '*',   0,
   ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0, 0x1E,   0,   0, 0x1C,   0, 0x1D,   0,   0, 0x1F,   0,   0,   0
};

bool keyboard_poll(char* out_char) {
	uint8_t status = inb(0x64);
	// Data must be available and NOT from auxiliary device (bit 0 set, bit 5 clear)
	if ((status & 1) == 0 || (status & 0x20) != 0) {
		return false;
	}

	uint8_t scancode = inb(0x60);

	// We only process press events (bit 7 clear)
	if (scancode & 0x80) {
		return false;
	}

	if (scancode < 128) {
		char c = scancode_map[scancode];
		if (c != 0) {
			*out_char = c;
			return true;
		}
	}

	return false;
}

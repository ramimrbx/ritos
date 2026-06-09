#include "../include/ritos/api.hpp"
#include "../../../kernel/include/kernel/memory.h"

namespace ritos {

void init() {
	// Initialize memory heap
	heap_init();
}

void print_message(const char* msg) {
	rit::System::print("[RitOS API] ");
	rit::System::println(msg);
}

void sleep(uint32_t count) {
	// Simple volatile delay loop to avoid optimization
	for (volatile uint32_t i = 0; i < count * 100000; i++) {
		__asm__ volatile("nop");
	}
}

} // namespace ritos

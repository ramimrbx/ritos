#include <ritos/api.hpp>
#include <kernel/memory.h>

namespace ritos {

void init() {
	// Heap is already initialized by kernel_main before fb_init allocates
	// the framebuffer back buffer; re-initializing here would reset the
	// bump allocator and alias future allocations over that buffer.
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

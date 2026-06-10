#include "../include/kernel/memory.h"

// Defined by linker script
extern uint8_t end[];

static uint8_t* heap_next = NULL;
static const size_t HEAP_MAX = 0x4000000; // 64MB Limit

void heap_init(void) {
	// Idempotent: a second call must not reset the bump pointer,
	// or earlier allocations would be handed out again.
	if (heap_next != NULL) return;
	heap_next = (uint8_t*)(((uintptr_t)end + 4095) & ~4095);
}

void* kmalloc(size_t size) {
	if (heap_next == NULL) {
		heap_init();
	}

	// Align requests to 4 bytes
	size = (size + 3) & ~3;

	// Simple check against out of bounds (assume we don't go past 64MB for this stage)
	if ((uintptr_t)(heap_next + size) > (uintptr_t)(end + HEAP_MAX)) {
		return NULL; // Out of memory
	}

	void* ptr = (void*)heap_next;
	heap_next += size;
	return ptr;
}

void kfree(void* ptr) {
	// For basic bump allocator, free is a no-op.
	(void)ptr;
}

size_t get_heap_usage(void) {
	if (heap_next == NULL) return 0;
	uint8_t* heap_start = (uint8_t*)(((uintptr_t)end + 4095) & ~4095);
	return (size_t)(heap_next - heap_start);
}

#include <stddef.h>

extern "C" {
	void* kmalloc(size_t size);
	void kfree(void* ptr);

	// Type definition for constructor functions
	typedef void (*constructor_t)();

	// Linker symbols
	extern constructor_t __init_array_start[];
	extern constructor_t __init_array_end[];

	// This function is called from boot.s to run global constructors
	void call_global_constructors() {
		for (constructor_t* ctor = __init_array_start; ctor < __init_array_end; ++ctor) {
			if (*ctor) {
				(*ctor)();
			}
		}
	}

	// Fallback function for pure virtual calls
	void __cxa_pure_virtual() {
		// Disable interrupts and halt
		while (1) {
			__asm__ volatile("cli; hlt");
		}
	}
}

// Global operator new and delete implementations

void* operator new(size_t size) {
	return kmalloc(size);
}

void* operator new[](size_t size) {
	return kmalloc(size);
}

void operator delete(void* ptr) noexcept {
	kfree(ptr);
}

void operator delete[](void* ptr) noexcept {
	kfree(ptr);
}

void operator delete(void* ptr, size_t size) noexcept {
	(void)size;
	kfree(ptr);
}

void operator delete[](void* ptr, size_t size) noexcept {
	(void)size;
	kfree(ptr);
}

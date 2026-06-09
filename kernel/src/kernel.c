#include "../include/kernel/terminal.h"
#include "../include/kernel/memory.h"
#include <stdint.h>

// Forward declarations of C++ system entry points
void ritos_init_framework(void);
void ritos_launch_gui(void);

void kernel_main(void) {
	// 1. Initialize the C OOP VGA Terminal
	Terminal_init(g_sys_terminal);

	// 2. Set color to Light Green on Black for banner
	Terminal_set_color(g_sys_terminal, VGA_COLOR_LIGHT_GREEN | (VGA_COLOR_BLACK << 4));

	// 3. Print the beautiful boot logo banner
	Terminal_writestring(g_sys_terminal, "  ___   _   _    ___   ___ \n");
	Terminal_writestring(g_sys_terminal, " | _ \\ (_) | |_ / _ \\ / __|\n");
	Terminal_writestring(g_sys_terminal, " |   / | | |  _| (_) |\\__ \\\n");
	Terminal_writestring(g_sys_terminal, " |_|_\\ |_|  \\__|\\___/ |___/\n\n");

	Terminal_writestring(g_sys_terminal, "==================================================\n");
	Terminal_writestring(g_sys_terminal, "               Welcome to RitOS v0.1.0            \n");
	Terminal_writestring(g_sys_terminal, "==================================================\n\n");

	Terminal_writestring(g_sys_terminal, "[Kernel] Initializing C OOP Terminal Device...\n");
	Terminal_writestring(g_sys_terminal, "[Kernel] Initializing Memory Manager (Bump Allocator)...\n");

	// Initialize the bump allocator heap
	heap_init();

	Terminal_writestring(g_sys_terminal, "[Kernel] Calling C++ SDK Framework Initialization...\n");

	// Initialize the C++ SDK Framework
	ritos_init_framework();

	Terminal_writestring(g_sys_terminal, "[Kernel] RitOS booted successfully!\n");
	
	// Print the requested text clearly
	Terminal_set_color(g_sys_terminal, VGA_COLOR_WHITE | (VGA_COLOR_BLACK << 4));
	Terminal_writestring(g_sys_terminal, "\nBoot Message: Booting RitOS...\n");
	Terminal_set_color(g_sys_terminal, VGA_COLOR_LIGHT_GREY | (VGA_COLOR_BLACK << 4));

	Terminal_writestring(g_sys_terminal, "\nTransitioning to C++ Desktop GUI in 3 seconds...\n");

	// Simple delay loop for ~3 seconds
	for (volatile uint32_t i = 0; i < 300000000; i++) {
		__asm__ volatile("nop");
	}

	// 4. Clear the screen and launch C++ Text GUI
	Terminal_clear(g_sys_terminal);
	ritos_launch_gui();

	// Infinite loop if GUI exits
	while (1) {
		__asm__ volatile("cli; hlt");
	}
}

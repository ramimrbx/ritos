#include "../include/kernel/power.h"
#include "../include/kernel/io.h"

void sys_reboot(void) {
	// 1. Reset CPU via PS/2 keyboard controller pulse
	outb(0x64, 0xFE);

	// 2. Fallback: Force CPU reset via Triple Fault
	__asm__ volatile (
		"lidt %0; int $3" 
		: 
		: "m"((uint16_t[3]){0, 0, 0})
	);

	// 3. Ultimate fallback: Hang CPU
	while (1) {
		__asm__ volatile("cli; hlt");
	}
}

void sys_shutdown(void) {
	// QEMU / Bochs (newer): write 0x2000 (SLP_EN | SLP_TYP) to PM1a_CNT
	outw(0x604, 0x2000);

	// QEMU / Bochs (older)
	outw(0xB004, 0x2000);

	// VirtualBox
	outw(0x4004, 0x3400);

	// Ultimate fallback: Hang CPU
	while (1) {
		__asm__ volatile("cli; hlt");
	}
}

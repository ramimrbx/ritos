/* 
Declare both Multiboot 1 and Multiboot 2 headers for dual BIOS and UEFI bootloader compatibility.
*/
.section .multiboot
.align 8
multiboot1_start:
.long 0x1BADB002                  /* Magic */
.long 0x00000003                  /* Flags (Align modules, memory info) */
.long -(0x1BADB002 + 0x00000003)  /* Checksum */

.align 8
multiboot2_start:
.long 0xE85250D6                  /* Magic number */
.long 0                           /* Architecture: i386 (protected mode) */
.long (multiboot2_end - multiboot2_start) /* Header length */
.long -(0xE85250D6 + 0 + (multiboot2_end - multiboot2_start)) /* Checksum */

/* Multiboot 2 Tags */
.align 8
.short 1                          /* Type: Info request */
.short 0                          /* Flags */
.long 8                           /* Size */

.align 8
.short 0                          /* Type: End */
.short 0                          /* Flags */
.long 8                           /* Size */
multiboot2_end:

/*
The multiboot standard does not define the value of the stack pointer register
(esp) and it is up to the kernel to provide a stack.
*/
.section .bss
.align 16
stack_bottom:
.skip 16384 /* 16 KiB stack */
stack_top:

/*
The linker script specifies _start as the entry point to the kernel and the
bootloader will jump to this position once the kernel has been loaded.
*/
.section .text
.global _start
.type _start, @function
_start:
	/* Set up our stack. */
	mov $stack_top, %esp

	/* Call the global constructors (C++ support). */
	call call_global_constructors

	/* Transfer control to the main kernel. */
	call kernel_main

	/*
	If the system has nothing more to do, put the computer into an
	infinite loop.
	*/
	cli
1:	hlt
	jmp 1b

/* Set size of _start symbol. */
.size _start, . - _start

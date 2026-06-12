/* 
Declare both Multiboot 1 and Multiboot 2 headers for dual BIOS and UEFI bootloader compatibility.
*/
.section .multiboot
.align 8
multiboot1_start:
.long 0x1BADB002                  /* Magic */
.long 0x00000007                  /* Flags: ALIGN | MEMINFO | VIDEO */
.long -(0x1BADB002 + 0x00000007)  /* Checksum */
.long 0                           /* header_addr  (ignored when bit16 is 0) */
.long 0                           /* load_addr */
.long 0                           /* load_end_addr */
.long 0                           /* bss_end_addr */
.long 0                           /* entry_addr */
.long 0                           /* mode_type: 0 = linear framebuffer */
.long 1920                        /* width  (bootloader falls back if unsupported) */
.long 1080                        /* height */
.long 32                          /* depth (bpp) */

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

/* Framebuffer tag: ask the bootloader (BIOS VBE or UEFI GOP alike) for a
   linear framebuffer; it falls back to the closest supported mode. */
.align 8
.short 5                          /* Type: Framebuffer */
.short 0                          /* Flags */
.long 20                          /* Size */
.long 1920                        /* Width */
.long 1080                        /* Height */
.long 32                          /* Depth (bpp) */

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

	/* Save Multiboot registers in callee-saved regs before any calls */
	mov %eax, %esi   /* magic number */
	mov %ebx, %edi   /* multiboot info pointer */

	/* Call the global constructors (C++ support). */
	call call_global_constructors

	/* Transfer control to the main kernel, passing saved Multiboot values. */
	push %edi    /* multiboot info pointer (arg2) */
	push %esi    /* magic number (arg1) */
	call kernel_main
	add $8, %esp

	/*
	If the system has nothing more to do, put the computer into an
	infinite loop.
	*/
	cli
1:	hlt
	jmp 1b

/* Set size of _start symbol. */
.size _start, . - _start

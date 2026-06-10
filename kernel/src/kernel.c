#include "../include/kernel/terminal.h"
#include "../include/kernel/memory.h"
#include "../include/kernel/fb.h"
#include "../include/kernel/io.h"
#include <stdint.h>

/* Serial port debug helpers (COM1 = 0x3F8) */
static void serial_init(void) {
    outb(0x3F8+1, 0x00); outb(0x3F8+3, 0x80);
    outb(0x3F8+0, 0x03); outb(0x3F8+1, 0x00);
    outb(0x3F8+3, 0x03); outb(0x3F8+2, 0xC7); outb(0x3F8+4, 0x0B);
}
static void serial_putc(char c) {
    while ((inb(0x3F8+5) & 0x20) == 0) {}
    outb(0x3F8, c);
}
static void serial_puts(const char* s) { while (*s) serial_putc(*s++); }
static void serial_hex(uint32_t v) {
    serial_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t n = (v >> i) & 0xF;
        serial_putc(n < 10 ? '0'+n : 'A'+n-10);
    }
}

/* Multiboot1 info struct – only fields we need */
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;        /* 0x58 */
    uint32_t framebuffer_pitch;       /* 0x60 */
    uint32_t framebuffer_width;       /* 0x64 */
    uint32_t framebuffer_height;      /* 0x68 */
    uint8_t  framebuffer_bpp;         /* 0x6C */
    uint8_t  framebuffer_type;        /* 0x6D */
    uint8_t  color_info[6];           /* 0x6E: red_pos,red_sz,grn_pos,grn_sz,blu_pos,blu_sz */
} __attribute__((packed)) multiboot_info_t;

#define MBI_FLAG_FB (1u << 12)

/* Forward declarations of C++ system entry points */
void ritos_init_framework(void);
void ritos_launch_gui(void);

void kernel_main(uint32_t magic, void* mbi_ptr) {
    serial_init();
    serial_puts("\r\nRitOS kernel_main\r\n");
    serial_puts("magic="); serial_hex(magic); serial_puts("\r\n");
    serial_puts("mbi="); serial_hex((uint32_t)mbi_ptr); serial_puts("\r\n");

    /* 1. Initialize heap first (fb_init needs kmalloc) */
    heap_init();

    /* 2. Try to set up VESA framebuffer */
    if (magic == 0x2BADB002 && mbi_ptr != 0) {
        multiboot_info_t* mbi = (multiboot_info_t*)mbi_ptr;
        serial_puts("mbi_flags="); serial_hex(mbi->flags); serial_puts("\r\n");

        if (mbi->flags & MBI_FLAG_FB) {
            serial_puts("fb_addr="); serial_hex((uint32_t)mbi->framebuffer_addr);
            serial_puts(" pitch="); serial_hex(mbi->framebuffer_pitch);
            serial_puts(" w="); serial_hex(mbi->framebuffer_width);
            serial_puts(" h="); serial_hex(mbi->framebuffer_height);
            serial_puts(" bpp="); serial_hex(mbi->framebuffer_bpp);
            serial_puts(" type="); serial_hex(mbi->framebuffer_type);
            serial_puts("\r\n");
        } else {
            serial_puts("no framebuffer flag in mbi\r\n");
        }

        if ((mbi->flags & MBI_FLAG_FB) &&
            mbi->framebuffer_type == 1 &&   /* type 1 = RGB linear */
            mbi->framebuffer_bpp >= 24) {
            fb_init(mbi->framebuffer_addr,
                    mbi->framebuffer_pitch,
                    mbi->framebuffer_width,
                    mbi->framebuffer_height,
                    mbi->framebuffer_bpp);
            serial_puts("fb_init done, fb_is_available=");
            serial_hex(fb_is_available()); serial_puts("\r\n");
        }
    } else {
        serial_puts("not MB1 or no mbi\r\n");
    }

    /* 3. Boot splash then launch GUI */
    if (fb_is_available()) {
        serial_puts("drawing boot splash\r\n");
        fb_clear_back(0xFF0D1B3Eu);  /* deep navy */

        /* Centered splash card */
        int cx = (fb_get_width()  - 240) / 2;
        int cy = (fb_get_height() - 72)  / 2;
        /* Outer glow */
        fb_fill_rounded_rect(cx - 4, cy - 4, 248, 80, 10, 0xFF1A2A5Eu);
        /* Card body */
        fb_fill_rounded_rect(cx, cy, 240, 72, 8, 0xFF1E2D5Au);
        /* Top accent bar */
        fb_fill_rounded_rect(cx, cy, 240, 4, 2, 0xFF5B7FFFu);
        /* Logo text */
        fb_draw_string("RitOS",      0xFF8BAEF0u, 0xFF1E2D5Au, cx + 8, cy + 12, 0);
        fb_draw_string("Loading...", 0xFF5E7BC4u, 0xFF1E2D5Au, cx + 8, cy + 40, 0);

        fb_flush();
        serial_puts("boot splash done\r\n");
    } else {
        Terminal_init(g_sys_terminal);
        Terminal_set_color(g_sys_terminal,
            VGA_COLOR_LIGHT_GREEN | (VGA_COLOR_BLACK << 4));
        Terminal_writestring(g_sys_terminal, "RitOS booting...\n");
    }

    serial_puts("ritos_init_framework start\r\n");
    ritos_init_framework();
    serial_puts("ritos_launch_gui start\r\n");
    ritos_launch_gui();
    serial_puts("ritos_launch_gui returned\r\n");

    while (1) { __asm__ volatile("cli; hlt"); }
}

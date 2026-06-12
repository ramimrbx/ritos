#include <kernel/terminal.h>
#include <kernel/memory.h>
#include <kernel/framebuffer.h>
#include <kernel/input_output.h>
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

/* Multiboot2 boot information: a sized header followed by 8-byte-aligned
 * tags. Tag type 8 carries the framebuffer (this is the only path a UEFI
 * bootloader hands us a screen on — there is no VBE under EFI). */
#define MB2_MAGIC       0x36D76289u
#define MB2_TAG_END     0
#define MB2_TAG_FB      8

typedef struct {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t  fb_type;            /* 1 = direct RGB */
    uint8_t  reserved[2];
} __attribute__((packed)) mb2_tag_fb_t;

/* Forward declarations of C++ system entry points */
void ritos_init_framework(void);
void ritos_launch_gui(void);

static void try_fb_init(uint64_t addr, uint32_t pitch, uint32_t w, uint32_t h,
                        uint8_t bpp, uint8_t type) {
    serial_puts("fb_addr="); serial_hex((uint32_t)addr);
    serial_puts(" pitch="); serial_hex(pitch);
    serial_puts(" w="); serial_hex(w);
    serial_puts(" h="); serial_hex(h);
    serial_puts(" bpp="); serial_hex(bpp);
    serial_puts(" type="); serial_hex(type);
    serial_puts("\r\n");
    if (type == 1 && bpp >= 24) {
        fb_init(addr, pitch, w, h, bpp);
        serial_puts("fb_init done, fb_is_available=");
        serial_hex(fb_is_available()); serial_puts("\r\n");
    }
}

void kernel_main(uint32_t magic, void* mbi_ptr) {
    serial_init();
    serial_puts("\r\nRitOS kernel_main\r\n");
    serial_puts("magic="); serial_hex(magic); serial_puts("\r\n");
    serial_puts("mbi="); serial_hex((uint32_t)mbi_ptr); serial_puts("\r\n");

    /* 1. Initialize heap first (fb_init needs kmalloc) */
    heap_init();

    /* 2. Try to set up the framebuffer from the boot info */
    if (magic == 0x2BADB002 && mbi_ptr != 0) {
        /* Multiboot 1 (BIOS / GRUB legacy path) */
        multiboot_info_t* mbi = (multiboot_info_t*)mbi_ptr;
        serial_puts("mbi_flags="); serial_hex(mbi->flags); serial_puts("\r\n");
        if (mbi->flags & MBI_FLAG_FB) {
            try_fb_init(mbi->framebuffer_addr, mbi->framebuffer_pitch,
                        mbi->framebuffer_width, mbi->framebuffer_height,
                        mbi->framebuffer_bpp, mbi->framebuffer_type);
        } else {
            serial_puts("no framebuffer flag in mbi\r\n");
        }
    } else if (magic == MB2_MAGIC && mbi_ptr != 0) {
        /* Multiboot 2 (UEFI / GRUB EFI path): walk the tag list */
        serial_puts("multiboot2 info\r\n");
        uint8_t* p   = (uint8_t*)mbi_ptr;
        uint32_t total = *(uint32_t*)p;
        uint8_t* end = p + total;
        p += 8;                                   /* total_size + reserved */
        while (p + 8 <= end) {
            uint32_t type = *(uint32_t*)p;
            uint32_t size = *(uint32_t*)(p + 4);
            if (type == MB2_TAG_END || size < 8) break;
            if (type == MB2_TAG_FB && size >= sizeof(mb2_tag_fb_t)) {
                mb2_tag_fb_t* fb = (mb2_tag_fb_t*)p;
                try_fb_init(fb->addr, fb->pitch, fb->width, fb->height,
                            fb->bpp, fb->fb_type);
            }
            p += (size + 7) & ~7u;                /* tags are 8-aligned */
        }
    } else {
        serial_puts("unknown boot magic / no mbi\r\n");
    }

    /* 3. Boot splash then launch GUI */
    if (fb_is_available()) {
        serial_puts("drawing boot splash\r\n");
        fb_clear_back(0xFF0C0C0Cu);  /* near-black, Windows-style boot */

        /* Centered logo + loading text, Instrument Sans */
        int sw = fb_get_width(), sh = fb_get_height();
        int tw = fb_text_width("RitOS", FB_FONT_H1);
        fb_draw_text("RitOS", (sw - tw) / 2, sh / 2 - 80, FB_FONT_H1, 0xFFFFFFFFu);

        /* Soft spinner dots */
        int dots_y = sh / 2 + 20;
        for (int i = 0; i < 5; i++)
            fb_fill_circle(sw / 2 - 28 + i * 14, dots_y, 3,
                           i == 2 ? 0xFFFFFFFFu : 0x60FFFFFFu);

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

#include <modern_gui/vbe.h>

static uint32_t* g_framebuffer = (uint32_t*)0;
static int g_fb_width = 0;
static int g_fb_height = 0;
static int g_fb_bpp = 0;
static uint32_t g_fb_pitch = 0;

void vbe_init(struct multiboot_info* mbi, uint32_t magic) {
	/* Multiboot 1 Magic Check */
	if (magic != 0x2BADB002) {
		return;
	}

	/* Check if linear framebuffer info is provided by the bootloader (flag bit 12) */
	if (mbi->flags & (1 << 12)) {
		g_framebuffer = (uint32_t*)(uint32_t)mbi->framebuffer_addr;
		g_fb_width = mbi->framebuffer_width;
		g_fb_height = mbi->framebuffer_height;
		g_fb_bpp = mbi->framebuffer_bpp;
		g_fb_pitch = mbi->framebuffer_pitch;
	} 
	/* Fallback: Check standard VBE mode info structure (flag bit 11) */
	else if (mbi->flags & (1 << 11)) {
		struct vbe_mode_info_block* mode_info = (struct vbe_mode_info_block*)mbi->vbe_mode_info;
		g_framebuffer = (uint32_t*)mode_info->physbase;
		g_fb_width = mode_info->width;
		g_fb_height = mode_info->height;
		g_fb_bpp = mode_info->bpp;
		g_fb_pitch = mode_info->pitch;
	}
}

uint32_t* vbe_get_framebuffer(void) {
	return g_framebuffer;
}

int vbe_get_width(void) {
	return g_fb_width;
}

int vbe_get_height(void) {
	return g_fb_height;
}

int vbe_get_bpp(void) {
	return g_fb_bpp;
}
